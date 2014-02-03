// ===============================================================
//  Includes
// ===============================================================

#include <stdio.h>
#include <stdlib.h>
#include "mem.h"
#include "objects.h"
#include "exceptions.h"
#include "arrays.h"
#include "code.h"
#include "vm.h"

#include "../runtime/runtime.h"

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

SepVM *vm_create(SepModule *module, SepObj *builtins) {
	SepVM *vm = malloc(sizeof(SepVM));

	// create the data stack
	vm->data = stack_create();
	vm->builtins = builtins;

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

	// repeat until we have a return value ready
	// (a break handles this from inside the body)
	while (true) {
		// grab a frame to execute
		ExecutionFrame *current_frame = &this->frames[this->frame_depth];
		// execute instructions
		frame_execute_instructions(current_frame, 1000);

		// did this frame call into another one?
		if (current_frame->called_another_frame) {
			// clear the flag
			current_frame->called_another_frame = false;
			// move down the stack
			// the call operation itself has already filled in the next frame
			this->frame_depth++;
		}

		// did we finish the frame?
		if (current_frame->finished) {
			// with an exception?
			if (sepv_is_exception(current_frame->return_value.value)) {
				// yup - we have to clean up
				this->frame_depth = 0;
				while (!stack_empty(this->data))
					stack_pop_item(this->data);

				// and pass the exception to the authorities
				return current_frame->return_value.value;
			}

			// nope, just a normal return
			// pop the frame
			this->frame_depth--;
			// push the return value on the data stack of its parent
			if (this->frame_depth >= starting_depth) {
				ExecutionFrame *parent_frame = &this->frames[this->frame_depth];
				stack_push_item(parent_frame->data, current_frame->return_value);
			} else {
				// this is the end, return the result of the whole execution
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
	frame->next_frame = &this->frames[1];

	// make the root object for the module
	SepObj *module_root = obj_create();

	// give it the proper prototypes
	SepArray *prototypes = array_create(2);
	array_push(prototypes, obj_to_sepv(this->builtins));
	array_push(prototypes, obj_to_sepv(proto_Object));
	module_root->prototypes = obj_to_sepv(prototypes);

	// set it as the execution scope for the root function
	frame->locals = obj_to_sepv(module_root);

	// give it the proper properties
	props_add_field(module_root, "locals", frame->locals);
	props_add_field(module_root, "this", frame->locals);

	// find the root function itself
	CodeBlock *root_block = bpool_block(module->blocks, 1);
	InterpretedFunc *root_func = ifunc_create(root_block, module_root);
	frame->function = (SepFunc*)root_func;

	// perform function specific initialization for the root function
	root_func->base.vt->initialize_frame((SepFunc*)root_func, frame);
}

// Initializes an execution frame for running a given function.
void vm_initialize_frame_for(SepVM *this, ExecutionFrame *frame, SepFunc *func, SepV locals) {
	// setup the frame
	frame->vm = this;
	frame->function = func;
	frame->data = this->data;
	frame->return_value = item_rvalue(SEPV_NOTHING);
	frame->finished = false;
	frame->called_another_frame = false;

	// create an execution scope
	frame->locals = locals;

	// build the prototype chain to give code access to everything it needs
	if (sepv_is_obj(locals)) {
		SepObj *locals_obj = sepv_to_obj(locals);
		// take prototypes based on the function
		SepV this_ptr = func->vt->get_this_pointer(func);
		SepObj *scope_object = func->vt->get_declaration_scope(func);
		SepArray *prototypes = array_create(4);
		array_push(prototypes, obj_to_sepv(this->builtins));
		if (this_ptr != SEPV_NOTHING)
			array_push(prototypes, this_ptr);
		if (scope_object)
			array_push(prototypes, obj_to_sepv(scope_object));
		array_push(prototypes, obj_to_sepv(proto_Object));

		// set the prototypes property on the local scope
		locals_obj->prototypes = obj_to_sepv(prototypes);

		// enrich the locals object with properties pointing to important objects
		props_add_field(locals_obj, "locals", locals);
		if (this_ptr != SEPV_NOTHING)
			props_add_field(locals_obj, "this", this_ptr);
	}

	// frames are contiguous in memory, so the next frame is right after
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

// Uses the VM to resolve a lazy value.
SepV vm_resolve(SepVM *this, SepFunc *func) {
	// setup a new frame as if this was a call
	this->frame_depth++;
	ExecutionFrame *frame = &this->frames[this->frame_depth];
	vm_initialize_frame_for(this, frame, func, obj_to_sepv(obj_create()));

	// run from that point until 'func' returns
	SepV return_value = vm_run(this);

	// return its return value
	return return_value;
}

SepV vm_resolve_as_literal(SepVM *this, SepFunc *func) {
	// setup a new frame as if this was a call
	this->frame_depth++;
	ExecutionFrame *frame = &this->frames[this->frame_depth];
	vm_initialize_frame_for(this, frame, func, SEPV_LITERALS);

	// run from that point until 'func' returns
	SepV return_value = vm_run(this);

	// return its return value
	return return_value;
}
