/*****************************************************************
 **
 ** runtime/functions.c
 **
 ** Implementation for September functions.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#include "mem.h"
#include "gc.h"
#include "arrays.h"
#include "opcodes.h"
#include "functions.h"
#include "exceptions.h"
#include "vm.h"
#include "support.h"

#include "../common/errors.h"
#include "../vm/runtime.h"

// ===============================================================
//  Function parameters
// ===============================================================

// Sets the value of the parameter in a given execution scope object. Signals problems
// by returning an exception SepV.
SepV funcparam_set_in_scope(ExecutionFrame *frame, FuncParam *this, SepObj *scope, SepV value) {
	// resolve lazy closures
	if (sepv_is_lazy(value) && (!this->flags.lazy)) {
		// a lazy argument and an eager parameter - trigger resolution
			value = vm_resolve(frame->vm, value);
			if (sepv_is_exception(value))
				return value;
	}

	// pass value in the right manner
	if (this->flags.sink) {
		// we have a '...' sink parameter - use an array
		SepArray *sink_array;
		if (props_find_prop(scope, this->name)) {
			// the array is present already
			sink_array = sepv_to_array(props_get_prop(scope, this->name));
		} else {
			// new array
			sink_array = array_create(1);
			props_accept_prop(scope, this->name, field_create(obj_to_sepv(sink_array)));
		}
		// push the value into the array representing the parameter
		array_push(sink_array, value);
	} else {
		// standard parameter, just set the value
		props_accept_prop(scope, this->name, field_create(value));
	}

	// everything OK
	return SEPV_NOTHING;
}

// Finalizes the value of the parameter - this is where default parameter
// values are set and parameters are validated.
SepV funcparam_finalize_value(ExecutionFrame *frame, SepFunc *func, FuncParam *this, SepObj *scope) {
	if (!props_find_prop(scope, this->name)) {
		// parameter missing

		SepV default_value = SEPV_NO_VALUE;

		// default value?
		if (this->flags.optional) {
			CodeUnit ref = this->default_value_reference;
			if (ref >= 0) {
				// this is a constant - get from the constant pool of the function
				default_value = cpool_constant(func->module->constants, ref);
			} else {
				// this is a lazy expression - call it in the function declaration scope
				SepV scope = func->vt->get_declaration_scope(func);
				CodeBlock *block = bpool_block(func->module->blocks, -ref);
				SepFunc *default_value_l = (SepFunc*)lazy_create(block, scope);
				default_value = vm_resolve(frame->vm, func_to_sepv(default_value_l));
			}
		}

		// sink parameters always have at least an empty array inside, even if no
		// default value was given
		if (default_value == SEPV_NO_VALUE && this->flags.sink) {
			default_value = obj_to_sepv(array_create(0));
		}

		if (default_value != SEPV_NO_VALUE) {
			// we have arrived at some default value to assign
			props_accept_prop(scope, this->name, field_create(default_value));
			return SEPV_NOTHING;
		} else {
			// no default value to be found, that's an error
			return sepv_exception(exc.EWrongArguments, sepstr_sprintf(
				"Required parameter '%s' is missing.", this->name->cstr
			));
		}
	}

	// nothing wrong with the parameter
	return SEPV_NOTHING;
}

// Sets up the all the call arguments inside the execution scope. Also validates them.
// Any problems found will be reported as an exception SepV in the return value.
SepV funcparam_pass_arguments(ExecutionFrame *frame, SepFunc *func, SepObj *scope, ArgumentSource *arguments) {
	// grab the argument count
	argcount_t arg_count = arguments->vt->argument_count(arguments);

	FuncParam *parameters = func->vt->get_parameters(func);
	argcount_t param_count = func->vt->get_parameter_count(func);

	// put arguments into the execution scope
	argcount_t arg_index = 0, param_index = 0;
	for (; arg_index < arg_count; arg_index++) {
		SepV argument = arguments->vt->get_next_argument(arguments);
		if (sepv_is_exception(argument))
			return argument;

		FuncParam *param = &parameters[param_index];

		// set the argument in scope
		SepV setting_exc = funcparam_set_in_scope(frame, param, scope, argument);
		if (sepv_is_exception(setting_exc))
			return setting_exc;

		// next parameter (unless this is the sink)
		if (!param->flags.sink)
			param_index++;
	}

	// finalize and validate parameters
	for (param_index = 0; param_index < param_count; param_index++) {
		FuncParam *param = &parameters[param_index];
		SepV result = funcparam_finalize_value(frame, func, param, scope);
			or_propagate_sepv(result);
	}

	// everything ok
	return SEPV_NOTHING;
}

// ===============================================================
//  Registering functions on creation
// ===============================================================

void _sepfunc_register(SepFunc *this) {
	ExecutionFrame *current_frame = vm_current_frame();
	if (current_frame)
		frame_register(current_frame, func_to_sepv(this));
}

// ===============================================================
//  Built-in function v-table
// ===============================================================

void _built_in_initialize_frame(SepFunc *_this, ExecutionFrame *frame) {
	// nothing for now
}

int _built_in_execute_instructions(SepFunc *this, ExecutionFrame *frame, int limit) {
	// extract what to execute on what
	BuiltInImplFunc implementation = ((BuiltInFunc*)this)->implementation;
	if (!sepv_is_obj(frame->locals)) {
		frame_raise(frame, sepv_exception(exc.EInternal,
				sepstr_for("Built-ins cannot be called in custom scopes.")));
		return 1;
	}
	SepObj *scope = sepv_to_obj(frame->locals);
	
	// run the built-in, giving it the scope from the stack frame, then retrieve result
	SepItem result = implementation(scope, frame);
	
	// finalize the stack frame (if not already finalized)
	if (!frame->finished)
		frame_return(frame, result);
	
	// each built-in counts as one instruction executed
	return 1;
}

uint8_t _built_in_get_parameter_count(SepFunc *this) {
	return ((BuiltInFunc*)this)->parameter_count;
}

FuncParam *_built_in_get_parameters(SepFunc *this) {
	return ((BuiltInFunc*)this)->parameters;
}

SepV _built_in_get_declaration_scope(SepFunc *this) {
	return SEPV_NOTHING;
}

SepV _built_in_get_this_pointer(SepFunc *this) {
	return SEPV_NOTHING;
}

void _built_in_mark_and_queue(SepFunc *this, GarbageCollection *gc) {
	BuiltInFunc *func = (BuiltInFunc*)this;

	// the parameter array is allocated dynamically, mark it and all parameter names in it
	if (func->parameters) {
		gc_mark_region(func->parameters);
		uint8_t p;
		for (p = 0; p < func->parameter_count; p++) {
			FuncParam *param = &func->parameters[p];
			gc_add_to_queue(gc, str_to_sepv(param->name));
		}
	}

	// additional data
	gc_add_to_queue(gc, func->data);
}

// v-table for built-ins
SepFuncVTable built_in_func_vtable = {
	&_built_in_initialize_frame,
	&_built_in_execute_instructions,
	&_built_in_get_parameter_count,
	&_built_in_get_parameters,
	&_built_in_get_declaration_scope,
	&_built_in_get_this_pointer,
	&_built_in_mark_and_queue
};

// ===============================================================
//  Built-in functions
// ===============================================================

// Checks if a parameter name starts with a given flag, and if it does,
// moves the parameter_name pointer past it.
bool parameter_extract_flag(char **parameter_name, const char *flag) {
	bool flag_set = strncmp(flag, *parameter_name, strlen(flag)) == 0;
	if (flag_set) {
		(*parameter_name) += strlen(flag); // move past the flag
		return true;
	} else {
		return false;
	}
}

// creates a new built-in based on a C function and September parameter names
BuiltInFunc *builtin_create_va(BuiltInImplFunc implementation, uint8_t parameters, va_list args) {
	// allocate and setup basic properties
	BuiltInFunc *built_in = mem_allocate(sizeof(BuiltInFunc));

	// make sure all unallocated pointers are NULL to avoid GC tripping over
	// uninitialized pointers
	built_in->parameters = NULL;
	built_in->data = SEPV_NOTHING;
	built_in->additional_pointer = NULL;

	built_in->base.vt = &built_in_func_vtable;
	built_in->base.lazy = false;
	built_in->base.module = NULL;
	built_in->parameter_count = parameters;
	built_in->implementation = implementation;

	// register to avoid accidental GC
	_sepfunc_register((SepFunc*)built_in);

	// setup parameters	according to va list
	built_in->parameters = mem_allocate(sizeof(FuncParam)*parameters);

	int i;
	char *param_name;
	for (i = 0; i < parameters; i++) {
		FuncParam *parameter = &built_in->parameters[i];

		// get name from va_list
		param_name = va_arg(args, char*);

		// extract flag
		parameter->flags.sink = parameter_extract_flag(&param_name, "...");
		parameter->flags.lazy = parameter_extract_flag(&param_name, "?");
		parameter->flags.optional = 0; // no way to make optional parameters in builtins for now

		// set the name (undecorated by now, the decoration got translated into flags)
		parameter->name = sepstr_for(param_name);
	}

	// return
	return built_in;
}

// creates a new built-in based on a C function and September parameter names
BuiltInFunc *builtin_create(BuiltInImplFunc implementation, uint8_t parameters, ...) {
	// delegate to _va version
	va_list args;
	va_start(args, parameters);
	BuiltInFunc *func = builtin_create_va(implementation, parameters, args);
	va_end(args);
	
	return func;
}
	
// ===============================================================
//  Interpreted function v-table
// ===============================================================

void _interpreted_initialize_frame(SepFunc *_this, ExecutionFrame *frame) {
	// quick reference
	InterpretedFunc *this = (InterpretedFunc*)_this;

	// set up instruction pointer
	frame->module = this->block->module;
	frame->instruction_ptr = this->block->instructions;
}

int _interpreted_execute_instructions(SepFunc *this, ExecutionFrame *frame, int limit) {
	// start executing from the block
	CodeUnit *end = ((InterpretedFunc*)this)->block->instructions_end;
	int instructions_left = limit;

	// repeat until one of the following happens:
	// 1) we run out of instructions (reached the end of the function)
	// 2) our frame is marked as finished (a return was executed or an
	//    exception was thrown)
	// 3) our frame spawns a new one (a function call)
	// 4) we run enough instructions to hit the limit
	while (instructions_left
		&& (frame->instruction_ptr < end)
		&& (!frame->finished)
		&& (!frame->called_another_frame))
	{

		// execute instruction
		int16_t opcode = frame_read(frame);
		InstructionLogic instruction = instruction_lut[opcode];
		instruction(frame);

		// next
		instructions_left--;
	}

	// out of instructions in the block?
	if (frame->instruction_ptr >= end)
		frame->finished = true;

	// we're done - how many did we execute?
	return limit - instructions_left;
}

uint8_t _interpreted_get_parameter_count(SepFunc *this) {
	// delegate to block
	return ((InterpretedFunc*)this)->block->parameter_count;
}

FuncParam *_interpreted_get_parameters(SepFunc *this) {
	// delegate to block
	return ((InterpretedFunc*)this)->block->parameters;
}

SepV _interpreted_get_declaration_scope(SepFunc *this) {
	return ((InterpretedFunc*)this)->declaration_scope;
}

SepV _interpreted_get_this_pointer(SepFunc *this) {
	return SEPV_NOTHING;
}

void _interpreted_mark_and_queue(SepFunc *this, GarbageCollection *gc) {
	InterpretedFunc *func = (InterpretedFunc*)this;
	// we can reach our declaration scope
	gc_add_to_queue(gc, func->declaration_scope);
}

// v-table for built-ins
SepFuncVTable interpreted_func_vtable = {
	&_interpreted_initialize_frame,
	&_interpreted_execute_instructions,
	&_interpreted_get_parameter_count,
	&_interpreted_get_parameters,
	&_interpreted_get_declaration_scope,
	&_interpreted_get_this_pointer,
	&_interpreted_mark_and_queue
};

// ===============================================================
//  Interpreted functions - public
// ===============================================================

// Creates a new closure for a given piece of code,
// closing over a provided scope.
InterpretedFunc *ifunc_create(CodeBlock *block, SepV declaration_scope) {
	InterpretedFunc *func = mem_allocate(sizeof(InterpretedFunc));
	func->base.vt = &interpreted_func_vtable;
	func->base.lazy = false;
	func->base.module = block->module;
	func->block = block;
	func->declaration_scope = declaration_scope;

	// register to avoid accidental GC
	_sepfunc_register((SepFunc*)func);

	return func;
}

// ===============================================================
//  Lazy closures
// ===============================================================

// exact copy of the ifunc vtable
SepFuncVTable lazy_closure_vtable = {
	&_interpreted_initialize_frame,
	&_interpreted_execute_instructions,
	&_interpreted_get_parameter_count,
	&_interpreted_get_parameters,
	&_interpreted_get_declaration_scope,
	&_interpreted_get_this_pointer,
	&_interpreted_mark_and_queue
};

// Creates a new lazy closure for a given expression.
InterpretedFunc *lazy_create(CodeBlock *block, SepV declaration_scope) {
	InterpretedFunc *func = ifunc_create(block, declaration_scope);
	func->base.lazy = true;
	return func;
}

// Checks whether a given SepV is a lazy closure.
bool sepv_is_lazy(SepV sepv) {
	if (!sepv_is_func(sepv)) return false;
	SepFunc *func = sepv_to_func(sepv);
	return func->lazy;
}

// Checks whether a given SepFunc* is a lazy closure.
bool func_is_lazy(SepFunc *func) {
	return func->lazy;
}

// ===============================================================
//  Bound method v-table
// ===============================================================

void _bm_initialize_frame(SepFunc *this, ExecutionFrame *frame) {
	SepFunc *original = ((BoundMethod*)this)->original_instance;
	original->vt->initialize_frame(original, frame);
}

int _bm_execute_instructions(SepFunc *this, ExecutionFrame *frame, int limit) {
	SepFunc *original = ((BoundMethod*)this)->original_instance;
	return original->vt->execute_instructions(original, frame, limit);
}

uint8_t _bm_get_parameter_count(SepFunc *this) {
	SepFunc *original = ((BoundMethod*)this)->original_instance;
	return original->vt->get_parameter_count(original);
}

FuncParam *_bm_get_parameters(SepFunc *this) {
	SepFunc *original = ((BoundMethod*)this)->original_instance;
	return original->vt->get_parameters(original);
}

SepV _bm_get_declaration_scope(SepFunc *this) {
	SepFunc *original = ((BoundMethod*)this)->original_instance;
	return original->vt->get_declaration_scope(original);
}

void _bm_mark_and_queue(SepFunc *this, GarbageCollection *gc) {
	BoundMethod *method = (BoundMethod*)this;
	// we can reach the 'this' object and our original function instance
	gc_add_to_queue(gc, method->this_pointer);
	gc_add_to_queue(gc, func_to_sepv(method->original_instance));
}

SepV _bm_get_this_pointer(SepFunc *this) {
	return ((BoundMethod*)this)->this_pointer;
}


// v-table for bound methods
SepFuncVTable bound_method_vtable = {
	&_bm_initialize_frame,
	&_bm_execute_instructions,
	&_bm_get_parameter_count,
	&_bm_get_parameters,
	&_bm_get_declaration_scope,
	&_bm_get_this_pointer,
	&_bm_mark_and_queue
};

// ===============================================================
//  Bound methods
// ===============================================================

// Creates a new bound method based on a given free function.
BoundMethod *boundmethod_create(SepFunc *function, SepV this_pointer) {
	BoundMethod *bm = mem_allocate(sizeof(BoundMethod));
	bm->base.vt = &bound_method_vtable;
	bm->base.module = function->module;
	bm->base.lazy = false;
	bm->original_instance = function;
	bm->this_pointer = this_pointer;

	// register to avoid accidental GC
	_sepfunc_register((SepFunc*)bm);

	return bm;
}
