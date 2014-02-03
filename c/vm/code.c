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
#include "../common/debugging.h"
#include "code.h"
#include "types.h"
#include "objects.h"
#include "arrays.h"
#include "exceptions.h"
#include "vm.h"

// ===============================================================
//  Instruction implementations
// ===============================================================

void push_const_impl(ExecutionFrame *frame) {
	uint32_t index = (uint32_t)frame_read(frame);
	log("opcodes", "push %d", index);
	stack_push_rvalue(frame->data, frame_constant(frame, index));
}

void lazy_call_impl(ExecutionFrame *frame) {
	// quick references
	SepStack *stack = frame->data;

	// what are we calling?
	SepFunc *func = sepv_to_func(stack_pop_value(stack));

	// verify argument/parameter relationship
	uint8_t arg_count = (uint8_t)frame_read(frame);
	log("opcodes", "lazy <%d args>", arg_count);

	FuncParam *parameters = func->vt->get_parameters(func);
	uint8_t param_count = func->vt->get_parameter_count(func);
	if (arg_count != param_count) {
		SepString *message = sepstr_sprintf("Expected %d arguments, called with %d.", param_count, arg_count);
		frame_raise(frame, sepv_exception(NULL, message));
		return;
	}

	// put arguments into the execution scope
	SepObj *execution_scope = obj_create();
	uint8_t arg_index;
	for (arg_index = 0; arg_index < arg_count; arg_index++) {
		CodeUnit argument_code = frame_read(frame);
		FuncParam *param = &parameters[arg_index];

		// did we get a constant or a block?
		if (argument_code > 0) {
			// constant, that's easy!
			SepV const_value = frame_constant(frame, argument_code);
			props_accept_prop(execution_scope, param->name, field_create(const_value));
		} else {
			// block - that's a lazy evaluated argument
			SepFunc *lazy = (SepFunc*)ifunc_create(
					frame_block(frame, -argument_code),
					sepv_to_obj(frame->locals));

			if (param->flags.lazy) {
				// the parameter is also lazy - we'll pass the function for later evaluation
				props_accept_prop(execution_scope, param->name, field_create(func_to_sepv(lazy)));
			} else {
				// the argument is eager - evaluate right now (in current scope)
				// and we'll pass the return value to the parameter
				SepFunc *func = (SepFunc*)ifunc_create(
						frame_block(frame, -argument_code),
						sepv_to_obj(frame->locals)); // TODO: check this
				SepV resolved_value = vm_resolve(frame->vm, func);
				if (sepv_is_exception(resolved_value)) {
					frame_raise(frame, resolved_value);
					return;
				}
				props_accept_prop(execution_scope, param->name, field_create(resolved_value));
			}
		}
	}

	// initialize a new frame for the function being called
	vm_initialize_frame_for(frame->vm, frame->next_frame, func, obj_to_sepv(execution_scope));

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
	SepItem property_value = sepv_get(host, property);
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
				sepv_exception(NULL, sepstr_create("Attempted assignment to an r-value.")));
		return;
	}

	// store the value in the slot specified
	slot->vt->store(slot, SEPV_NOTHING, value);

	// return the value to the stack (as an rvalue)
	stack_push_rvalue(frame->data, value);
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
	&push_locals_impl, &fetch_prop_impl, &pop_impl, &store_impl
};
