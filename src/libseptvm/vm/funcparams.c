/*****************************************************************
 **
 ** vm/funcargs.c
 **
 ** All code dealing with how arguments of a function call end up
 ** in the callee's execution scope resides in this source file.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdbool.h>
#include "types.h"
#include "exceptions.h"
#include "arrays.h"
#include "objects.h"
#include "vm.h"
#include "runtime.h"
#include "support.h"

// ===============================================================
//  Bytecode argument source
// ===============================================================

Argument *_bytecode_next_argument(ArgumentSource *_this) {
	BytecodeArgs *this = (BytecodeArgs*)_this;
	ExecutionFrame *frame = this->source_frame;

	// are we done?
	if (this->argument_index >= this->argument_count)
		return NULL;

	// nope, read the next reference
	CodeUnit arg_reference = frame_read(frame);
	PoolReferenceType ref_type = decode_reference_type(arg_reference);
	uint32_t ref_index = decode_reference_index(arg_reference);
	this->argument_index++;

	// handle named arguments
	if (ref_type == PRT_ARGUMENT_NAME) {
		this->current_arg.name = sepv_to_str(frame_constant(frame, ref_index));
		// skip to next reference to get the value for the name
		arg_reference = frame_read(frame);
		ref_type = decode_reference_type(arg_reference);
		ref_index = decode_reference_index(arg_reference);
		this->argument_index++;
	} else {
		this->current_arg.name = NULL;
	}

	// argument value - did we get a constant or a block?
	switch(ref_type) {
		case PRT_CONSTANT: {
			// constant, that's easy!
			this->current_arg.value = frame_constant(frame, ref_index);
			break;
		}
		case PRT_FUNCTION: {
			// block - that's a lazy evaluated argument
			CodeBlock *block = frame_block(frame, ref_index);
			if (!block) {
				this->current_arg.value = sepv_exception(exc.EInternal,
						sepstr_sprintf("Code block %d out of bounds.", ref_index));
				return &this->current_arg;
			}
			SepFunc *argument_l = (SepFunc*)lazy_create(block, frame->locals);
			this->current_arg.value = func_to_sepv(argument_l);
			break;
		}
		default:
			this->current_arg.value = sepv_exception(exc.EInternal, sepstr_sprintf("Unrecognized reference type: %d", ref_type));
	}

	// return a pointer into our own structure
	return &this->current_arg;
}

ArgumentSourceVT bytecode_args_vt = {
	&_bytecode_next_argument
};

// Initializes a new bytecode source in place.
void bytecodeargs_init(BytecodeArgs *this, ExecutionFrame *frame) {
	this->base.vt = &bytecode_args_vt;
	this->source_frame = frame;
	this->argument_index = 0;
	this->argument_count = (argcount_t)frame_read(frame);
}

// ===============================================================
//  va_list argument source
// ===============================================================

Argument *_va_next_argument(ArgumentSource *_this) {
	VAArgs *this = (VAArgs*)_this;

	// keep track of the count
	if (this->argument_index >= this->argument_count)
		return NULL;
	this->argument_index++;

	// return next SepV from the va_list
	this->current_arg.value = va_arg(this->c_arg_list, SepV);
	return &this->current_arg;
}

ArgumentSourceVT va_args_vt = {
	&_va_next_argument
};

// Initializes a new VAArgs source in place.
void vaargs_init(VAArgs *this, argcount_t arg_count, va_list args) {
	this->base.vt = &va_args_vt;
	this->argument_index = 0;
	this->argument_count = arg_count;
	this->current_arg.name = NULL; // no names for VAArgs

	va_copy(this->c_arg_list, args);
}

// ===============================================================
//  Array-based argument source
// ===============================================================

Argument *_arrayargs_next_argument(ArgumentSource *_this) {
	ArrayArgs *this = (ArrayArgs*)_this;
	if (arrayit_end(&this->iterator))
		return NULL;
	this->current_arg.value = arrayit_next(&this->iterator);
	return &this->current_arg;
}

ArgumentSourceVT array_args_vt = {
	&_arrayargs_next_argument
};

// Initializes a new VAArgs source in place.
void arrayargs_init(ArrayArgs *this, SepArray *array) {
	this->base.vt = &array_args_vt;
	this->array = array;
	this->iterator = array_iterate_over(array);
	this->current_arg.name = NULL; // no named arguments yet
}

// ===============================================================
//  Setting individual parameters
// ===============================================================

// Puts the value of a standard (non-sink, single-valued) parameter in the execution scope.
SepV funcparam_set_in_standard_parameter(FuncParam *param, SepObj *scope, SepV argument_value) {
	if (props_find_prop(scope, param->name)) {
		// duplicate parameter
		if (param->flags.type != PT_STANDARD_PARAMETER) {
			return sepv_exception(exc.EWrongArguments,
					sepstr_sprintf("Values for sink parameter '%s' provided both implicitly and explicitly.", param->name->cstr));
		} else {
			return sepv_exception(exc.EWrongArguments,
					sepstr_sprintf("Parameter '%s' was passed more than once in a function call.", param->name->cstr));
		}
	}
	props_add_prop(scope, param->name, &st_field, argument_value);
	return SEPV_NOTHING;
}

// Puts the value of a positional sink (...args) parameter in the execution scope.
SepV funcparam_set_in_positional_sink(FuncParam *param, SepObj *scope, SepV argument_value) {
	SepArray *sink_array;
	if (props_find_prop(scope, param->name)) {
		// the array is present already
		sink_array = sepv_to_array(props_get_prop(scope, param->name));
	} else {
		// new array
		sink_array = array_create(1);
		props_add_prop(scope, param->name, &st_field, obj_to_sepv(sink_array));
	}
	// push the value into the array representing the parameter
	array_push(sink_array, argument_value);
	return SEPV_NOTHING;
}

// Puts the value of a named sink (:::props) parameter in the execution scope.
SepV funcparam_set_in_named_sink(FuncParam *this, SepObj *scope, SepString *argument_name, SepV argument_value) {
	SepObj *sink_obj;
	if (props_find_prop(scope, this->name)) {
		// the sink object is present already
		sink_obj = sepv_to_obj(props_get_prop(scope, this->name));
	} else {
		// make a new object and store it
		sink_obj = obj_create();
		props_add_prop(scope, this->name, &st_field, obj_to_sepv(sink_obj));
	}
	// set the property on the sink object
	if (props_find_prop(sink_obj, argument_name)) {
		return sepv_exception(exc.EWrongArguments,
				sepstr_sprintf("Parameter '%s' was passed more than once in a function call.", this->name->cstr));
	}
	props_add_prop(sink_obj, argument_name, &st_field, argument_value);
	return SEPV_NOTHING;
}


// Resolves lazy arguments if we're trying to put them in eager parameters.
SepV funcparam_resolve_argument_if_needed(ExecutionFrame *frame, FuncParam *param, SepV argument_value) {
	if (sepv_is_lazy(argument_value) && (!param->flags.lazy)) {
		// a lazy argument and an eager parameter - trigger resolution
		return vm_resolve(frame->vm, argument_value);
	} else {
		// no resolution needed
		return argument_value;
	}
}

// Sets the value of the parameter in a given execution scope object. Signals problems
// by returning an exception SepV.
SepV funcparam_set_in_scope(ExecutionFrame *frame, FuncParam *param, SepObj *scope, Argument *argument) {
	// extract the value
	SepV value = argument->value;

	// resolve lazy values if necessary at this point
	value = funcparam_resolve_argument_if_needed(frame, param, value);
		or_raise_sepv(value);

	// did we arrive at this parameter by directly naming it in the call?
	bool directly_named = argument->name && !sepstr_cmp(argument->name, param->name);
	// pass value in the right manner for the parameter
	if (param->flags.type == PT_STANDARD_PARAMETER || directly_named) {
		// standard parameter, just set the value
		// we also treat sink parameters this way if they were
		// explicitly named in the call, eg. func(sink: [1,2,3])
		return funcparam_set_in_standard_parameter(param, scope, value);
	} else if (param->flags.type == PT_NAMED_SINK) {
		return funcparam_set_in_named_sink(param, scope, argument->name, value);
	} else if (param->flags.type == PT_POSITIONAL_SINK) {
		return funcparam_set_in_positional_sink(param, scope, value);
	} else {
		return sepv_exception(exc.EInternal,
				sepstr_sprintf("Unrecognized paramer type for parameter '%s'.", param->name));
	}

	// everything OK
	return SEPV_NOTHING;
}

// ===============================================================
//  Parameter validation and finalization
// ===============================================================

// Finalizes the value of the parameter - this is where default parameter
// values are set and parameters are validated.
SepV funcparam_finalize_value(ExecutionFrame *frame, SepFunc *func, FuncParam *this, SepObj *scope) {
	// is the parameter missing?
	if (!props_find_prop(scope, this->name)) {
		// let's see if we can find a default value for it
		SepV default_value = SEPV_NO_VALUE;
		bool default_value_found = false;

		// default value provided?
		if (this->flags.optional) {
			// we'll definitely find one
			default_value_found = true;
			// is this a built-in (and has no module pointer?)
			if (func->module) {
				CodeUnit dv_reference = this->default_value_reference;
				PoolReferenceType dv_type = decode_reference_type(dv_reference);
				uint32_t dv_index = decode_reference_index(dv_reference);
				switch (dv_type) {
					case PRT_CONSTANT:
						default_value = cpool_constant(func->module->constants, dv_index);
						break;
					case PRT_FUNCTION: {
						// this is a lazy expression - call it in the function declaration scope
						SepV scope = func->vt->get_declaration_scope(func);
						CodeBlock *block = bpool_block(func->module->blocks, dv_index);
						SepFunc *default_value_l = (SepFunc*)lazy_create(block, scope);
						default_value = vm_resolve(frame->vm, func_to_sepv(default_value_l));
						break;
					}
					default:
						return sepv_exception(exc.EInternal, sepstr_new("Default value references can only be constants or functions."));
				}
			} else {
				// this function has no module, so it is a built-in - and built-ins use SEPV_NO_VALUE for all defaults
				default_value = SEPV_NO_VALUE;
			}
		}

		// sink parameters always have an implicit default value even if none was given
		if (!default_value_found && this->flags.type == PT_POSITIONAL_SINK) {
			default_value = obj_to_sepv(array_create(0));
			default_value_found = true;
		}
		if (!default_value_found && this->flags.type == PT_NAMED_SINK) {
			default_value = obj_to_sepv(obj_create());
			default_value_found = true;
		}

		if (default_value_found) {
			// we have arrived at some default value to assign
			props_add_prop(scope, this->name, &st_field, default_value);
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

// ===============================================================
//  Internal helpers for the argument passing process
// ===============================================================

// Finds the parameter corresponding to a named argument, or returns NULL if there is none.
FuncParam *funcparam_find_parameter_for_named_argument(FuncParam *parameters, argcount_t param_count, Argument *argument) {
	// named argument, find the right parameter
	FuncParam *param;
	int p;
	FuncParam *named_sink = NULL;
	for (p = 0; p < param_count; p++) {
		param = &parameters[p];
		if (!sepstr_cmp(param->name, argument->name))
			return param;
		if (param->flags.type == PT_NAMED_SINK)
			named_sink = param;
	}
	// we've run out of parameters - our only hope is a sink
	if (named_sink) {
		return named_sink;
	} else {
		return NULL;
	}
}

// Checks whether this argument should advance our position on the parameter list.
bool funcparam_argument_advances_position(FuncParam *parameter, Argument *argument) {
	// named arguments don't move us forward
	if (argument->name) return false;
	// sink parameters accept any number of values
	if (parameter->flags.type != PT_STANDARD_PARAMETER) return false;
	// seems like a standard positional argument if we reached here
	return true;
}

// ===============================================================
//  Argument passing - public interface
// ===============================================================

// Sets up the all the call arguments inside the execution scope. Also validates them.
// Any problems found will be reported as an exception SepV in the return value.
SepV funcparam_pass_arguments(ExecutionFrame *frame, SepFunc *func, SepObj *scope, ArgumentSource *arguments) {
	// grab the argument count
	FuncParam *parameters = func->vt->get_parameters(func);
	argcount_t param_count = func->vt->get_parameter_count(func);

	// put arguments into the execution scope
	argcount_t position = 0;
	Argument *argument = arguments->vt->get_next_argument(arguments);
	while (argument) {
		SepV value = argument->value;
		if (sepv_is_exception(value))
			return value;

		// find the parameter we should use for this argument
		FuncParam *parameter;
		if (argument->name) {
			// named argument, find the right parameter to put it in
			parameter = funcparam_find_parameter_for_named_argument(parameters, param_count, argument);
			if (!parameter) {
				return sepv_exception(exc.EWrongArguments,
					sepstr_sprintf("Named argument '%s' does not match any parameter.", argument->name->cstr));
			}
		} else {
			// positional argument, find next positional parameter
			if (position >= param_count) {
				return sepv_exception(exc.EWrongArguments,
					sepstr_sprintf("Too many arguments specified."));
			}
			parameter = &parameters[position];
		}

		// put the argument in scope
		SepV setting_exc = funcparam_set_in_scope(frame, parameter, scope, argument);
			or_raise_sepv(setting_exc);

		// move to next positional parameter if needed
		if (funcparam_argument_advances_position(parameter, argument))
			position++;

		// next argument
		argument = arguments->vt->get_next_argument(arguments);
	}

	// finalize and validate all parameters
	int p;
	for (p = 0; p < param_count; p++) {
		FuncParam *param = &parameters[p];
		SepV result = funcparam_finalize_value(frame, func, param, scope);
			or_raise_sepv(result);
	}

	// everything ok
	return SEPV_NOTHING;
}
