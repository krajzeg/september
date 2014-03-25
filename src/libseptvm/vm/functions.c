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
#include "arrays.h"
#include "opcodes.h"
#include "functions.h"
#include "exceptions.h"
#include "vm.h"

#include "../common/errors.h"
#include "../vm/runtime.h"

// ===============================================================
//  Function parameters
// ===============================================================

// Sets the value of the parameter in a given execution scope object.
void funcparam_set_in_scope(FuncParam *this, SepObj *scope, SepV value) {
	if (this->flags.sink) {
		// a '...' sink parameter - always an array
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
}

// Finalizes the value of the parameter - this is where default parameter
// values are set and parameters are validated.
void funcparam_finalize_value(FuncParam *this, SepObj *scope, SepError *out_err) {
	if (!props_find_prop(scope, this->name)) {
		// parameter missing

		// sink parameters always have at least an empty array inside
		if (this->flags.sink) {
			SepV sink_array_v = obj_to_sepv(array_create(0));
			props_accept_prop(scope, this->name, field_create(sink_array_v));
			return;
		}

		// no default value to be found, that's an error
		fail(error_create(EWrongArguments, "Required parameter '%s' is missing.",
				this->name->cstr));
	}
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

// v-table for built-ins
SepFuncVTable built_in_func_vtable = {
	&_built_in_initialize_frame,
	&_built_in_execute_instructions,
	&_built_in_get_parameter_count,
	&_built_in_get_parameters,
	&_built_in_get_declaration_scope,
	&_built_in_get_this_pointer
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
	built_in->base.vt = &built_in_func_vtable;
	built_in->parameter_count = parameters;
	built_in->parameters = mem_allocate(sizeof(FuncParam)*parameters);
	built_in->implementation = implementation;

	// setup parameters	according to va list
	int i;
	char *param_name;
	for (i = 0; i < parameters; i++) {
		FuncParam *parameter = &built_in->parameters[i];

		// get name from va_list
		param_name = va_arg(args, char*);

		// extract flag
		parameter->flags.sink = parameter_extract_flag(&param_name, "...");
		parameter->flags.lazy = parameter_extract_flag(&param_name, "?");

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

// v-table for built-ins
SepFuncVTable interpreted_func_vtable = {
	&_interpreted_initialize_frame,
	&_interpreted_execute_instructions,
	&_interpreted_get_parameter_count,
	&_interpreted_get_parameters,
	&_interpreted_get_declaration_scope,
	&_interpreted_get_this_pointer
};

// ===============================================================
//  Interpreted functions - public
// ===============================================================

// Creates a new closure for a given piece of code,
// closing over a provided scope.
InterpretedFunc *ifunc_create(CodeBlock *block, SepV declaration_scope) {
	InterpretedFunc *func = mem_allocate(sizeof(InterpretedFunc));
	func->base.vt = &interpreted_func_vtable;
	func->block = block;
	func->declaration_scope = declaration_scope;
	return func;
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
	&_bm_get_this_pointer
};

// ===============================================================
//  Bound methods
// ===============================================================

// Creates a new bound method based on a given free function.
BoundMethod *boundmethod_create(SepFunc *function, SepV this_pointer) {
	BoundMethod *bm = mem_allocate(sizeof(BoundMethod));
	bm->base.vt = &bound_method_vtable;
	bm->original_instance = function;
	bm->this_pointer = this_pointer;
	return bm;
}
