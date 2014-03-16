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

#include "code.h"
#include "types.h"
#include "objects.h"
#include "arrays.h"
#include "exceptions.h"
#include "vm.h"

#include "../runtime/support.h"
#include "../common/debugging.h"

// ===============================================================
//  Instruction implementations
// ===============================================================

void push_const_impl(ExecutionFrame *frame) {
	CodeUnit index = (CodeUnit)frame_read(frame);
	log("opcodes", "push %d", index);

	SepV value;
	if (index > 0) {
		// just a constant
		value = frame_constant(frame, index);
	} else {
		// oh goody, an anonymous function!
		CodeBlock *block = frame_block(frame, -index);
		if (block == NULL) {
			value = sepv_exception(builtin_exception("EInternalError"), sepstr_sprintf("Code block %d is out of bounds.", -index));
		} else {
			SepFunc *func = (SepFunc*)ifunc_create(block, frame->locals);
			value = func_to_sepv(func);
		}
	}
	if (sepv_is_exception(value))
		frame_raise(frame, value);
	else
		stack_push_rvalue(frame->data, value);
}

void lazy_call_impl(ExecutionFrame *frame) {
	SepError err = NO_ERROR;

	// quick references
	SepStack *stack = frame->data;
	SepFunc *func = sepv_to_func(stack_pop_value(stack));

	// verify argument/parameter relationship
	uint8_t arg_count = (uint8_t)frame_read(frame);
	log("opcodes", "lazy <%d args>", arg_count);

	FuncParam *parameters = func->vt->get_parameters(func);
	uint8_t param_count = func->vt->get_parameter_count(func);

	// put arguments into the execution scope
	SepObj *execution_scope = obj_create();
	uint8_t arg_index = 0, param_index = 0;
	for (; arg_index < arg_count; arg_index++) {
		CodeUnit argument_code = frame_read(frame);
		FuncParam *param = &parameters[param_index];

		// did we get a constant or a block?
		SepV argument;
		if (argument_code > 0) {
			// constant, that's easy!
			argument = frame_constant(frame, argument_code);
		} else {
			// block - that's a lazy evaluated argument
			CodeBlock *block = frame_block(frame, -argument_code);
			if (!block) {
				frame_raise(frame, sepv_exception(builtin_exception("EInternalError"),
						sepstr_sprintf("Code block %d out of bounds.", -argument_code)));
				return;
			}
			SepFunc *argument_l = (SepFunc*)ifunc_create(block, frame->locals);

			if (param->flags.lazy) {
				// the parameter is also lazy - we'll pass the function for later evaluation
				argument = func_to_sepv(argument_l);
			} else {
				// the argument is eager - evaluate right now (in current scope)
				// and we'll pass the return value to the parameter
				argument = vm_resolve(frame->vm, func_to_sepv(argument_l));
				// propagate exception, if any
				if (sepv_is_exception(argument)) {
					frame_raise(frame, argument);
					return;
				}
			}

		}

		// set the argument in scope
		funcparam_set_in_scope(param, execution_scope, argument);

		// next parameter (unless this is the sink)
		if (!param->flags.sink)
			param_index++;
	}

	// finalize and validate parameters
	for (param_index = 0; param_index < param_count; param_index++) {
		FuncParam *param = &parameters[param_index];
		funcparam_finalize_value(param, execution_scope, &err);
			or_handle(EAny) {
				frame_raise(frame, sepv_exception(
						builtin_exception("EWrongArguments"),
						sepstr_create(err.message)));
			};
	}

	// add a 'return' function
	BuiltInFunc *return_f = make_return_func(frame->next_frame);
	obj_add_field(execution_scope, "return", func_to_sepv(return_f));

	// initialize a new frame for the function being called
	vm_initialize_scope(frame->vm, func, execution_scope);
	vm_initialize_frame(frame->vm, frame->next_frame, func, obj_to_sepv(execution_scope));

	// we're making a call, so let the VM know about that
	frame->called_another_frame = true;
}

void push_locals_impl(ExecutionFrame *frame) {
	log0("opcodes", "pushlocals");
	stack_push_rvalue(frame->data, frame->locals);
}

void fetch_prop_impl(ExecutionFrame *frame) {
	// get the object to fetch from
	SepV host = stack_pop_value(frame->data);

	// get the property name
	int16_t index = frame_read(frame);
	SepString *property = sepv_to_str(frame_constant(frame, index));
	log("opcodes", "fetchprop %d(%s)", index, sepstr_to_cstr(property));

	// retrieve the value
	SepItem property_value = sepv_get_item(host, property);
	if (sepv_is_exception(property_value.value)) {
		frame_raise(frame, property_value.value);
		return;
	}

	// push it on the stack
	stack_push_item(frame->data, property_value);
}

void store_impl(ExecutionFrame *frame) {
	log0("opcodes", "store");

	// get the value
	SepV value = stack_pop_value(frame->data);
	// get the slot to set stuff in
	Slot *slot = stack_pop_item(frame->data).origin;
	if (!slot) {
		frame_raise(frame,
				sepv_exception(builtin_exception("ECannotAssign"), sepstr_create("Attempted assignment to an r-value.")));
		return;
	}

	// store the value in the slot specified
	slot->vt->store(slot, SEPV_NOTHING, value);

	// return the value to the stack (as an rvalue)
	stack_push_rvalue(frame->data, value);
}

void create_field_impl(ExecutionFrame *frame) {
	// get the object to fetch from
	SepV host_v = stack_pop_value(frame->data);

	// get the property name
	int16_t index = frame_read(frame);
	SepString *property = sepv_to_str(frame_constant(frame, index));
	log("opcodes", "createprop %d(%s)", index, sepstr_to_cstr(property));

	// ensure the host is an object
	if (!sepv_is_obj(host_v)) {
		frame_raise(frame, sepv_exception(builtin_exception("EWrongType"),
				sepstr_create("New properties can only be created on objects, not primitives.")));
		return;
	}
	SepObj *host = sepv_to_obj(host_v);

	// check if the field exists
	if (props_find_prop(host, property)) {
		SepString *message = sepstr_sprintf("Property '%s' already exists and cannot be created.",
				sepstr_to_cstr(property));
		frame_raise(frame, sepv_exception(builtin_exception("EPropertyExists"), message));
		return;
	}

	// it doesn't, so let's create it
	Slot *slot = props_accept_prop(host, property, field_create(SEPV_NOTHING));

	// push it on the stack
	SepItem property_item = item_lvalue(slot, SEPV_NOTHING);
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
