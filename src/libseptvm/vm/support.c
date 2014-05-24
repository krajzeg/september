/*****************************************************************
 **
 ** support.c
 **
 ** Implementation for support.h.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "../vm/exceptions.h"
#include "../vm/types.h"
#include "../vm/functions.h"
#include "../vm/arrays.h"
#include "../vm/runtime.h"
#include "../vm/support.h"

// ===============================================================
//  Safe parameter access
// ===============================================================

SepString *cast_as_str(SepV value, SepV *error) {
	return cast_as_named_str("Value", value, error);
}

SepObj *cast_as_obj(SepV value, SepV *error) {
	return cast_as_named_obj("Value", value, error);
}

SepFunc *cast_as_func(SepV value, SepV *error) {
	return cast_as_named_func("Value", value, error);
}

SepInt cast_as_int(SepV value, SepV *error) {
	return cast_as_named_int("Value", value, error);
}

SepString *cast_as_named_str(char *name, SepV value, SepV *error) {
	if (!sepv_is_str(value))
		fail(NULL, exception(exc.EWrongType, "%s is supposed to be a string."));
	else
		return sepv_to_str(value);
}

SepObj *cast_as_named_obj(char *name, SepV value, SepV *error) {
	if (!sepv_is_obj(value))
		fail(NULL, exception(exc.EWrongType, "%s is supposed to be an object."));
	else
		return sepv_to_obj(value);
}

SepFunc *cast_as_named_func(char *name, SepV value, SepV *error) {
	if (!sepv_is_func(value))
		fail(NULL, exception(exc.EWrongType, "%s is supposed to be a function."));
	else
		return sepv_to_func(value);
}

SepInt cast_as_named_int(char *name, SepV value, SepV *error) {
	if (!sepv_is_int(value))
		fail(0, exception(exc.EWrongType, "%s is supposed to be an integer."));
	else
		return sepv_to_int(value);
}

// ===============================================================
//  Operating on properties
// ===============================================================

// Adds a new slot of a chosen type to a given object. There are more convenient
// functions for specific slot types.
void obj_add_slot(SepObj *obj, char *name, SlotType *type, SepV value) {
	props_add_prop(obj, sepstr_for(name), type, value);
}

// Adds a new field to a given object.
void obj_add_field(SepObj *obj, char *name, SepV contents) {
	props_add_prop(obj, sepstr_for(name), &st_field, contents);
}

// Adds a new built-in method to a given object.
void obj_add_builtin_method(SepObj *obj, char *name, BuiltInImplFunc impl, uint8_t param_count, ...) {
	// create the built-in func object
	va_list args;
	va_start(args, param_count);
	SepFunc *builtin = (SepFunc*)builtin_create_va(impl, param_count, args);
	va_end(args);

	// make the slot
	props_add_prop(obj, sepstr_for(name), &st_method, func_to_sepv(builtin));
}

// Adds a new built-in free function (as opposed to a method) to a given object.
void obj_add_builtin_func(SepObj *obj, char *name, BuiltInImplFunc impl, uint8_t param_count, ...) {
	// create the built-in func object
	va_list args;
	va_start(args, param_count);
	SepFunc *builtin = (SepFunc*)builtin_create_va(impl, param_count, args);
	va_end(args);

	// make the slot
	props_add_prop(obj, sepstr_for(name), &st_field, func_to_sepv(builtin));
}

// Adds a new prototype to the object.
void obj_add_prototype(SepObj *obj, SepV prototype) {
	SepV current = obj->prototypes;
	if (current == SEPV_NOTHING) {
		// this is the first prototype, just set it
		obj->prototypes = prototype;
	} else if (sepv_is_array(current)) {
		// already an array of prototypes, just push the new one
		SepArray *array = (SepArray*)sepv_to_obj(current);
		array_push(array, prototype);
	} else {
		// object had one prototype - we have to create a new array with 2 elements
		// to accomodate the new prototype
		SepArray *array = array_create(2);
		array_push(array, current);
		array_push(array, prototype);
		obj->prototypes = obj_to_sepv(array);
	}
}

// Adds a new escape function (like "return", "break", etc.) to a local scope.
void obj_add_escape(SepObj *obj, char *name, ExecutionFrame *return_to_frame, SepV return_value) {
	// just add it as a field
	obj_add_field(obj, name, func_to_sepv(make_escape_func(return_to_frame, return_value)));
}

// ===============================================================
//  Accessing properties quickly
// ===============================================================

// Retrieves a property from a SepV using proper lookup.
// Function simplifying property retrieval with constant name,
// used instead of sepv_get when you don't have a SepStr.
SepV property(SepV host, char *name) {
	return sepv_get(host, sepstr_for(name));
}

// Checks whether a named property exists on a host object (including prototype lookup).
bool property_exists(SepV host, char *name) {
	Slot *slot = sepv_lookup(host, sepstr_for(name), NULL);
	return (slot != NULL);
}

// Calls a method from a SepV and returns the return value. Any problems
// (the method not being there, the property not being callable) are
// reported as an exception. Arguments have to be passed in as SepVs.
SepV call_method(SepVM *vm, SepV host, char *name, int argument_count, ...) {
	SepV method_v = property(host, name);
		or_raise_sepv(method_v);

	va_list args;
	va_start(args, argument_count);

	VAArgs arg_source;
	vaargs_init(&arg_source, argument_count, args);

	SepV result = vm_invoke_with_argsource(vm, method_v, SEPV_NO_VALUE, (ArgumentSource*)&arg_source).value;
	va_end(args);

	return result;
}

// ===============================================================
//  Classes and prototypes
// ===============================================================

// Creates a new class with the given name and parent class.
// The Class object must be already available in the runtime.
SepObj *make_class(char *name, SepObj *parent) {
	SepV parent_v;
	if (parent)
		parent_v = obj_to_sepv(parent);
	else
		parent_v = strcmp(name, "Object") ? obj_to_sepv(rt.Object) : SEPV_NOTHING;
	SepObj *cls = obj_create_with_proto(parent_v);

	// store the name permanently for future reference
	obj_add_field(cls, "<name>", str_to_sepv(sepstr_for(name)));
	obj_add_field(cls, "<class>", obj_to_sepv(cls));
	obj_add_field(cls, "<superclass>", parent_v);

	// copy properties from the Class master object
	SepV call_v = property(obj_to_sepv(rt.Cls), "<call>");
	props_add_prop(cls, sepstr_for("<call>"), &st_method, call_v);

	// return the class
	return cls;
}

// Checks whether a given object has another among its prototypes (or grand-prototypes).
bool has_prototype(SepV object, SepV requested) {
	SepV proto = sepv_prototypes(object);
	if (proto == SEPV_NOTHING)
		return false;

	if (sepv_is_simple_object(proto)) {
		// a simple object - not an array
		if (proto == requested)
			return true;
		// recurse
		if (proto != object)
			return has_prototype(proto, object);
	}

	if (sepv_is_array(proto)) {
		// iterate over all prototypes
		SepArrayIterator it = array_iterate_over(sepv_to_array(proto));
		while (!arrayit_end(&it)) {
			SepV prototype = arrayit_next(&it);
			if (prototype == requested)
				return true;
			if (has_prototype(prototype, requested))
				return true;
		}
		// options exhausted
		return false;
	}

	// it's strange that we're here, but we don't know how to find a prototype in this case
	return false;
}

// ===============================================================
//  Escape functions
// ===============================================================

typedef struct EscapeData {
	ExecutionFrame *original_frame;
	SepV return_value;
} EscapeData;

SepItem escape_impl(SepObj *scope, ExecutionFrame *frame) {
	// retrieve data from the function
	BuiltInFunc *bfunc = (BuiltInFunc*)frame->function;

	ExecutionFrame *escape_frame = bfunc->additional_pointer;
	SepItem return_value = item_rvalue(bfunc->data);

	// drop frames until we reach the right scope
	while (frame != NULL) {
		// finish the frame
		frame->finished = true;
		frame->return_value = return_value;

		// have we reached the frame we want to escape out of?
		if (frame == escape_frame)
			break;

		// we go one frame up if not
		frame = frame->prev_frame;
	}

	// return
	return return_value;
}

SepItem return_impl(SepObj *scope, ExecutionFrame *frame) {
	// retrieve the desired return value (using the default if none was provided)
	SepV return_value_v = param(scope, "return_value");
	if (return_value_v == SEPV_NO_VALUE)
		return_value_v = SEPV_NOTHING;
	SepItem return_value = item_rvalue(return_value_v);

	// retrieve data from the function
	BuiltInFunc *bfunc = (BuiltInFunc*)frame->function;

	ExecutionFrame *escape_frame = bfunc->additional_pointer;
	// drop frames until we reach the right scope
	while (frame != NULL) {
		// finish the frame
		frame->finished = true;
		frame->return_value = return_value;

		// have we reached the frame we want to escape out of?
		if (frame == escape_frame)
			break;

		// we go one frame up if not
		frame = frame->prev_frame;
	}

	// return
	return return_value;
}

BuiltInFunc *make_escape_func(ExecutionFrame *frame, SepV value_returned) {
	// create a new escape function
	BuiltInFunc *function = builtin_create(escape_impl, 0);

	// remember some stuff inside the function to be able to escape
	// to the right place with the right value later
	function->additional_pointer = frame;
	function->data = value_returned;

	// return the function
	return function;
}

BuiltInFunc *make_return_func(ExecutionFrame *frame) {
	// create a new return function
	BuiltInFunc *function = builtin_create(return_impl, 1, "=return_value");

	// remember some stuff inside the function to be able to escape
	// to the right place, leave return value to be filled later
	function->additional_pointer = frame;

	// return the function
	return function;
}
