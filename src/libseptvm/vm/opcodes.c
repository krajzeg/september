/*****************************************************************
 **
 ** vm/code.c
 **
 ** Implementation of VM instructions.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "opcodes.h"
#include "gc.h"
#include "types.h"
#include "objects.h"
#include "arrays.h"
#include "exceptions.h"
#include "functions.h"
#include "vm.h"

#include "../vm/runtime.h"
#include "../vm/support.h"
#include "../common/debugging.h"

// ===============================================================
//  References
// ===============================================================

// Given a reference from bytecode, returns its type.
PoolReferenceType decode_reference_type(CodeUnit reference) {
	if (reference >= 0)
		return PRT_CONSTANT;
	else
		return (-reference) & 0x1;
}

// Given a reference from bytecode, returns the index inside the pool that
// it is referring to.
uint32_t decode_reference_index(CodeUnit reference) {
	if (reference > 0)
		return reference;
	else
		return (-reference) >> 1;
}

// ===============================================================
//  Instruction implementations
// ===============================================================

void push_const_impl(ExecutionFrame *frame) {
	CodeUnit reference = (CodeUnit)frame_read(frame);
	log("opcodes", "push %d", reference);

	SepV value;

	PoolReferenceType ref_type = decode_reference_type(reference);
	uint32_t ref_index = decode_reference_index(reference);
	switch(ref_type) {
		case PRT_CONSTANT:
			value = frame_constant(frame, ref_index);
			break;
		case PRT_FUNCTION: {
			CodeBlock *block = frame_block(frame, ref_index);
			if (block == NULL) {
				value = sepv_exception(exc.EInternal, sepstr_sprintf("Code block %d is out of bounds.", ref_index));
			} else {
				SepFunc *func = (SepFunc*)ifunc_create(block, frame->locals);
				value = func_to_sepv(func);
			}
			break;
		}
		default:
			frame_raise(frame, sepv_exception(exc.EInternal, sepstr_new("Only constants or functions can be PUSHed.")));
	}

	if (sepv_is_exception(value))
		frame_raise(frame, value);
	else
		stack_push_rvalue(frame->data, value);
}

void lazy_call_impl(ExecutionFrame *frame) {
	// get reference to the function being called
	SepStack *stack = frame->data;
	SepFunc *func = sepv_call_target(stack_pop_value(stack));
	if (!func) {
		frame_raise(frame, sepv_exception(exc.EWrongType, sepstr_new("The object to be called is not a function or a callable.")));
		return;
	}

	// use the VM's BytecodeArgs object to avoid allocation
	BytecodeArgs bcargs;
	ArgumentSource *args = (ArgumentSource*)(&bcargs);
	bytecodeargs_init(&bcargs, frame);

	log0("opcodes", "lazy <? args>");

	// initialize a new frame for the function being called
	vm_initialize_frame(frame->vm, frame->next_frame, func);

	// temporarily switch the VM to the callee frame to make sure newly allocated objects
	// registered as GC roots of the callee, not the caller
	frame->vm->frame_depth++;

	// create execution scope
	SepObj *execution_scope = obj_create();
	SepV argument_exception = funcparam_pass_arguments(frame, func, execution_scope, args);
	if (sepv_is_exception(argument_exception)) {
		// we have to drop frame_depth back to the right level first
		frame->vm->frame_depth--;
		frame_raise(frame, argument_exception);
		return;
	}
	vm_initialize_scope(frame->vm, func, execution_scope, frame->next_frame);

	// restore VM to the proper state and raise the subcall flag
	frame->vm->frame_depth--;
	frame->called_another_frame = true;
}

void push_locals_impl(ExecutionFrame *frame) {
	log0("opcodes", "pushlocals");
	stack_push_rvalue(frame->data, frame->locals);
}

void fetch_prop_impl(ExecutionFrame *frame) {
	// get the object to fetch from
	// we don't pop it yet to keep a live reference to it on the stack
	// this will prevent it from being GC'd if a GC happens in the middle
	// of this method
	SepV host = stack_top_value(frame->data);

	// get the property name
	CodeUnit reference = frame_read(frame);
	uint32_t index = decode_reference_index(reference);
	SepString *property = sepv_to_str(frame_constant(frame, index));
	log("opcodes", "fetchprop %d(%s)", index, property->cstr);

	// retrieve the value
	SepItem property_value = sepv_get_item(host, property);
	if (sepv_is_exception(property_value.value)) {
		frame_raise(frame, property_value.value);
		return;
	}

	// replace the host with the fetched property on the stack
	stack_replace_top(frame->data, property_value);
}

void store_impl(ExecutionFrame *frame) {
	log0("opcodes", "store");

	// get the value
	SepV value = stack_pop_value(frame->data);
	// get the slot to set stuff in
	SepItem item = stack_pop_item(frame->data);
	if (!item_is_lvalue(item)) {
		frame_raise(frame,
				sepv_exception(exc.ECannotAssign, sepstr_for("Attempted assignment to an r-value.")));
		return;
	}

	// store the value in the slot specified
	SepV result = slot_store(item.slot, &item.origin, value);

	// return the value to the stack (as an rvalue)
	stack_push_rvalue(frame->data, result);
}

void create_field_impl(ExecutionFrame *frame) {
	// get the object to fetch from
	SepV host_v = stack_pop_value(frame->data);

	// get the property name
	CodeUnit reference = frame_read(frame);
	uint32_t index = decode_reference_index(reference);
	SepString *property = sepv_to_str(frame_constant(frame, index));
	log("opcodes", "createprop %d(%s)", index, property->cstr);

	// ensure the host is an object
	if (!sepv_is_obj(host_v)) {
		frame_raise(frame, sepv_exception(exc.EWrongType,
				sepstr_for("New properties can only be created on objects, not primitives.")));
		return;
	}
	SepObj *host = sepv_to_obj(host_v);

	// check if the field exists
	if (props_find_prop(host, property)) {
		SepString *message = sepstr_sprintf("Property '%s' already exists and cannot be created.",
				property->cstr);
		frame_raise(frame, sepv_exception(exc.EPropertyAlreadyExists, message));
		return;
	}

	// it doesn't, so let's create it
	Slot *slot = props_add_prop(host, property, &st_field, SEPV_NOTHING);

	// push it on the stack
	SepItem property_item = item_property_lvalue(host_v, host_v, property, slot, SEPV_NOTHING);
	stack_push_item(frame->data, property_item);
}

void pop_impl(ExecutionFrame *frame) {
	log0("opcodes", "pop");

	// just pop the top
	SepItem popped_value = stack_pop_item(frame->data);
	// latch as return value
	frame->return_value = popped_value;
}

// ===============================================================
//  Instruction lookup table
// ===============================================================

/**
 * Table for looking instructions up by opcode.
 */
InstructionLogic instruction_lut[OP_MAX] = {
	NULL, &push_const_impl, NULL, NULL,
	&lazy_call_impl, NULL, NULL, NULL,
	&push_locals_impl, &fetch_prop_impl, &pop_impl,
	&store_impl, &create_field_impl
};
