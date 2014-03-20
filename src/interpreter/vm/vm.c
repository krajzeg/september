// ===============================================================
//  Includes
// ===============================================================

#include <stdio.h>
#include <stdlib.h>
#include "mem.h"
#include "objects.h"
#include "exceptions.h"
#include "arrays.h"
#include "opcodes.h"
#include "vm.h"

#include "../common/debugging.h"
#include "../runtime/runtime.h"
#include "../runtime/support.h"

// ===============================================================
//  Execution frame
// ===============================================================

// Execute instructions from this frame, up to a given limit.
int frame_execute_instructions(ExecutionFrame *frame, int limit) {
	SepFunc *function = frame->function;
	return function->vt->execute_instructions(function, frame, limit);
}

// Reads the next code unit from this frame.
CodeUnit frame_read(ExecutionFrame *frame) {
	CodeUnit code = *(frame->instruction_ptr++);
	return code;
}

// Read a constant from this frame's module.
SepV frame_constant(ExecutionFrame *frame, uint32_t index) {
	return cpool_constant(frame->module->constants, index);
}

CodeBlock *frame_block(ExecutionFrame *frame, uint32_t index) {
	return bpool_block(frame->module->blocks, index);
}

// Finalize this frame with a given return value.
void frame_return(ExecutionFrame *frame, SepItem return_value) {
	frame->return_value = return_value;
	frame->finished = true;
}

// Raise an exception inside a frame and finalizes it.
void frame_raise(ExecutionFrame *frame, SepV exception) {
	frame->return_value = item_rvalue(exception);
	frame->finished = true;
}

// ===============================================================
//  The virtual machine
// ===============================================================

SepVM *vm_create(SepModule *module, SepObj *syntax) {
	SepVM *vm = malloc(sizeof(SepVM));

	// create the data stack
	vm->data = stack_create();
	vm->syntax = syntax;

	// start a frame for the root function and set it to be current
	vm_initialize_root_frame(vm, module);
	vm->frame_depth = 0;

	// return
	return vm;
}

SepV vm_run(SepVM *this) {
	// store the starting depth for this run - once we leave this level,
	// we stop execution and return the last return value
	int starting_depth = this->frame_depth;

	// mark our place on the stack so that we can unwind it properly
	// when exception is thrown
	stack_push_rvalue(this->data, SEPV_UNWIND_MARKER);

	// repeat until we have a return value ready
	// (a break handles this from inside the body)
	while (true) {
		// grab a frame to execute
		ExecutionFrame *current_frame = &this->frames[this->frame_depth];

		// execute instructions (if we're not done)
		if ((!current_frame->finished) && (!current_frame->called_another_frame))
			frame_execute_instructions(current_frame, 1000);

		// did this frame call into another one?
		if (current_frame->called_another_frame) {
			// clear the flag
			current_frame->called_another_frame = false;
			// move down the stack
			// the call operation itself has already filled in the next frame
			this->frame_depth++;
			// put an unwind marker on the stack
			stack_push_rvalue(this->data, SEPV_UNWIND_MARKER);

			log("vm", "(%d) New execution frame created (interpreted call).", this->frame_depth);
		}

		// did we finish the frame?
		if (current_frame->finished) {
			// with an exception?
			if (sepv_is_exception(current_frame->return_value.value)) {
				log("vm", "(%d) Execution frame finished with exception.", this->frame_depth);
				log("vm", "Unwinding to level (%d).", starting_depth);

				int frames_to_unwind = this->frame_depth - starting_depth + 1;

				// drop all the frames from this VM run
				this->frame_depth = starting_depth - 1;

				// clear data from the stack
				while (frames_to_unwind > 0) {
					SepV stack_value = stack_pop_value(this->data);
					if (stack_value == SEPV_UNWIND_MARKER)
						frames_to_unwind--;
				}

				// and pass the exception to the authorities
				return current_frame->return_value.value;
			}

			// nope, just a normal return
			log("vm", "(%d) Execution frame finished normally.", this->frame_depth);

			// pop the frame
			this->frame_depth--;
			// unwind the stack (usually it will be just the unwind marker, but in
			// case of a 'break' or 'return' it might be more items)
			SepV stack_value = stack_pop_value(this->data);
			while (stack_value != SEPV_UNWIND_MARKER)
				stack_value = stack_pop_value(this->data);

			// push the return value on the data stack of its parent
			if (this->frame_depth >= starting_depth) {
				ExecutionFrame *parent_frame = &this->frames[this->frame_depth];
				stack_push_item(parent_frame->data, current_frame->return_value);
			} else {
				// this is the end of this execution
				// return the result of the whole execution
				return current_frame->return_value.value;
			}
		}
	}
}

// Initializes the root execution frame based on a module.
void vm_initialize_root_frame(SepVM *this, SepModule *module) {
	// root function always starts at the 0th frame
	ExecutionFrame *frame = &this->frames[0];

	// setup the frame
	frame->vm = this;
	frame->data = this->data;
	frame->return_value = item_rvalue(SEPV_NOTHING);
	frame->finished = false;
	frame->called_another_frame = false;
	frame->module = module;
	frame->instruction_ptr = NULL;
	frame->prev_frame = NULL;
	frame->next_frame = &this->frames[1];

	// set it as the execution scope for the root function
	frame->locals = obj_to_sepv(module->root);

	// give it the proper properties
	props_add_field(module->root, "locals", frame->locals);
	props_add_field(module->root, "this", frame->locals);

	// find the root function itself
	CodeBlock *root_block = bpool_block(module->blocks, 1);
	InterpretedFunc *root_func = ifunc_create(root_block, frame->locals);
	frame->function = (SepFunc*)root_func;

	// perform function specific initialization for the root function
	root_func->base.vt->initialize_frame((SepFunc*)root_func, frame);
}

void vm_initialize_scope(SepVM *this, SepFunc *func, SepObj* exec_scope) {
	SepV exec_scope_v = obj_to_sepv(exec_scope);

	// take prototypes based on the function
	SepV this_ptr_v = func->vt->get_this_pointer(func);
	SepV decl_scope_v = func->vt->get_declaration_scope(func);
	SepArray *prototypes = array_create(4);
	array_push(prototypes, obj_to_sepv(this->syntax));
	if ((this_ptr_v != SEPV_NOTHING) && (this_ptr_v != exec_scope_v))
		array_push(prototypes, this_ptr_v);
	if ((decl_scope_v != SEPV_NOTHING) && (decl_scope_v != exec_scope_v))
		array_push(prototypes, decl_scope_v);
	array_push(prototypes, obj_to_sepv(rt.Object));

	// set the prototypes property on the local scope
	exec_scope->prototypes = obj_to_sepv(prototypes);

	// enrich the locals object with properties pointing to important objects
	props_add_field(exec_scope, "locals", exec_scope_v);
	if (this_ptr_v != SEPV_NOTHING)
		props_add_field(exec_scope, "this", this_ptr_v);
}

// Initializes an execution frame for running a given function.
void vm_initialize_frame(SepVM *this, ExecutionFrame *frame, SepFunc *func, SepV locals) {
	// setup the frame
	frame->vm = this;
	frame->function = func;
	frame->data = this->data;
	frame->return_value = item_rvalue(SEPV_NOTHING);
	frame->finished = false;
	frame->called_another_frame = false;

	// set an execution scope
	frame->locals = locals;

	// frames are contiguous in memory, so the next frame is right after
	frame->prev_frame = frame - 1;
	frame->next_frame = frame + 1;

	// we don't know the right values, but the function will initialize
	// those fields on its own if it cares about them
	frame->instruction_ptr = NULL;
	frame->module = NULL;

	// perform function specific initialization
	func->vt->initialize_frame(func, frame);
}

void vm_free(SepVM *this) {
	if (!this) return;
	free(this);
}

// ===============================================================
//  Subcalls and lazy argument resolution
// ===============================================================

// Makes a subcall from within a built-in function. The result of the subcall is then returned.
// Any number of parameters can be passed in, and they should be passed as SepVs.
SepItem vm_subcall(SepVM *this, SepFunc *func, uint8_t argument_count, ...) {
	va_list args;
	va_start(args, argument_count);
	SepItem result = vm_subcall_v(this, func, argument_count, args);
	va_end(args);
	return result;
}

// Makes a subcall from within a built-in function. The result of the subcall is then returned.
// A started va_list of SepVs should be passed in by another vararg function.
SepItem vm_subcall_v(SepVM *this, SepFunc *func, uint8_t argument_count, va_list args) {
	// verify parameter count
	uint8_t index;
	uint8_t param_count = func->vt->get_parameter_count(func);
	FuncParam *parameters = func->vt->get_parameters(func);
	if (param_count != argument_count)
		return si_exception(exc.EInternal, sepstr_sprintf("Argument count mismatch: expected %d arguments, got %d arguments.", param_count, argument_count));

	// put parameters in
	SepObj *scope = obj_create();
	for (index = 0; index < argument_count; index++) {
		SepV arg = va_arg(args, SepV);
		FuncParam *param = &parameters[index];
		props_accept_prop(scope, param->name, field_create(arg));
	}

	// initialize an execution frame
	this->frame_depth++;
	log("vm", "(%d) New execution frame created (subcall from b-in).", this->frame_depth);
	ExecutionFrame *frame = &this->frames[this->frame_depth];

	vm_initialize_scope(this, func, scope);
	vm_initialize_frame(this, frame, func, obj_to_sepv(scope));

	// run to get result
	SepV result = vm_run(this);

	return item_rvalue(result);
}

// Uses the VM to resolve a lazy value.
SepV vm_resolve(SepVM *this, SepV lazy_value) {
	if (sepv_is_func(lazy_value)) {
		// function - resolve it in it's default scope
		SepFunc *func = sepv_to_func(lazy_value);
		SepV scope = func->vt->get_declaration_scope(func);
		return vm_resolve_in(this, lazy_value, scope);
	} else {
		// not a function, no resolution needed
		return lazy_value;
	}
}

// Uses the VM to resolve a lazy value in a specified scope (instead
// of in its parent scope).
SepV vm_resolve_in(SepVM *this, SepV lazy_value, SepV scope) {
	// maybe it doesn't need resolving?
	if (!sepv_is_func(lazy_value))
		return lazy_value;

	// it does - extract the function and set up a frame for it
	this->frame_depth++;
	log("vm", "(%d) New execution frame created (value resolve).", this->frame_depth);

	SepFunc *func = sepv_to_func(lazy_value);
	ExecutionFrame *frame = &this->frames[this->frame_depth];
	vm_initialize_frame(this, frame, func, scope);

	// run from that point until 'func' returns
	SepV return_value = vm_run(this);

	// return its return value
	return return_value;
}

// Uses the VM to resolve a lazy value to the identifier that it names.
// This uses a special "Literals" scope in which every identifier resolves
// to itself.
SepV vm_resolve_as_literal(SepVM *this, SepV lazy_value) {
	return vm_resolve_in(this, lazy_value, SEPV_LITERALS);
}