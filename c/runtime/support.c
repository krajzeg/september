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
#include "../common/errors.h"
#include "../vm/exceptions.h"
#include "../vm/types.h"
#include "../vm/functions.h"
#include "../vm/arrays.h"
#include "runtime.h"
#include "support.h"

// ===============================================================
//  Safe parameter access
// ===============================================================

SepString *cast_as_str(SepV value, SepError *out_err) {
	return cast_as_named_str("Value", value, out_err);
}

SepObj *cast_as_obj(SepV value, SepError *out_err) {
	return cast_as_named_obj("Value", value, out_err);
}

SepFunc *cast_as_func(SepV value, SepError *out_err) {
	return cast_as_named_func("Value", value, out_err);
}

SepInt cast_as_int(SepV value, SepError *out_err) {
	return cast_as_named_int("Value", value, out_err);
}

SepString *cast_as_named_str(char *name, SepV value, SepError *out_err) {
	if (!sepv_is_str(value))
		fail(NULL, e_type_mismatch(name, "a string"));
	else
		return sepv_to_str(value);
}

SepObj *cast_as_named_obj(char *name, SepV value, SepError *out_err) {
	if (!sepv_is_obj(value))
			fail(NULL, e_type_mismatch(name, "an object"));
		else
			return sepv_to_obj(value);
}

SepFunc *cast_as_named_func(char *name, SepV value, SepError *out_err) {
	if (!sepv_is_func(value))
			fail(NULL, e_type_mismatch(name, "a function"));
		else
			return sepv_to_func(value);
}

SepInt cast_as_named_int(char *name, SepV value, SepError *out_err) {
	if (!sepv_is_int(value))
			fail(0, e_type_mismatch(name, "an integer"));
		else
			return sepv_to_int(value);
}

// ===============================================================
//  Operating on properties
// ===============================================================

// Adds a new field to a given object.
void obj_add_field(SepObj *obj, char *name, SepV contents) {
	props_accept_prop(obj, sepstr_create(name), field_create(contents));
}

// Adds a new built-in method to a given object.
void obj_add_builtin_method(SepObj *obj, char *name, BuiltInImplFunc impl, uint8_t param_count, ...) {
	// create the built-in func object
	va_list args;
	va_start(args, param_count);
	SepFunc *builtin = (SepFunc*)builtin_create_va(impl, param_count, args);
	va_end(args);

	// make the slot
	props_accept_prop(obj, sepstr_create(name), method_create(func_to_sepv(builtin)));
}

// Adds a new built-in free function (as opposed to a method) to a given object.
void obj_add_builtin_func(SepObj *obj, char *name, BuiltInImplFunc impl, uint8_t param_count, ...) {
	// create the built-in func object
	va_list args;
	va_start(args, param_count);
	SepFunc *builtin = (SepFunc*)builtin_create_va(impl, param_count, args);
	va_end(args);

	// make the slot
	props_accept_prop(obj, sepstr_create(name), field_create(func_to_sepv(builtin)));
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
//  Classes
// ===============================================================

// Creates a new class object with the given name and parent class.
SepObj *make_class(char *name, SepObj *parent) {
	SepV parent_v;
	if (parent)
		parent_v = obj_to_sepv(parent);
	else
		parent_v = strcmp(name, "Object") ? obj_to_sepv(proto_Object) : SEPV_NOTHING;

	SepObj *cls = obj_create_with_proto(parent_v);

	// store the name permanently for future reference
	obj_add_field(cls, "<name>", str_to_sepv(sepstr_create(name)));
	obj_add_field(cls, "<class>", obj_to_sepv(cls));
	obj_add_field(cls, "<superclass>", parent_v);

	// return the class
	return cls;
}

// ===============================================================
//  Exceptions
// ===============================================================

// Returns a built-in exception type.
SepObj *builtin_exception(char *name) {
	SepV value = props_get_prop(proto_Exceptions, sepstr_create(name));
	if (value == SEPV_NOTHING)
		return NULL;
	else
		return sepv_to_obj(value);
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
	EscapeData *data = ((BuiltInFunc*)frame->function)->data;

	ExecutionFrame *escape_frame = data->original_frame;
	SepItem return_value = item_rvalue(data->return_value);

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
	// set the return value in the 'escape' structure
	SepV return_value = param(scope, "return_value");
	EscapeData *data = ((BuiltInFunc*)frame->function)->data;
	data->return_value = return_value;

	// delegate to the standard escape functionality
	return escape_impl(scope, frame);
}

BuiltInFunc *make_escape_func(ExecutionFrame *frame, SepV value_returned) {
	// create a new escape function
	BuiltInFunc *function = builtin_create(escape_impl, 0);

	// remember some stuff inside the function to be able to escape
	// to the right place with the right value later
	EscapeData *escape_data = mem_allocate(sizeof(EscapeData));
	escape_data->original_frame = frame;
	escape_data->return_value = value_returned;
	function->data = escape_data;

	// return the function
	return function;
}

BuiltInFunc *make_return_func(ExecutionFrame *frame) {
	// create a new escape function
	BuiltInFunc *function = builtin_create(return_impl, 1, "return_value");

	// remember some stuff inside the function to be able to escape
	// to the right place, leave return value to be filled later
	EscapeData *escape_data = mem_allocate(sizeof(EscapeData));
	escape_data->original_frame = frame;
	function->data = escape_data;

	// return the function
	return function;
}

// ===============================================================
//  Common operations on SepVs
// ===============================================================

SepString *sepv_debug_string(SepV sepv, SepError *out_err) {
	SepError err = NO_ERROR;

	// look for the class
	Slot *class_slot = sepv_lookup(sepv, sepstr_create("<class>"));
	if (class_slot) {
		// retrieve the name of the class
		SepV class_v = class_slot->vt->retrieve(class_slot, sepv);
		SepV class_name_v = sepv_get(class_v, sepstr_create("<name>"));
		SepString *class_name = cast_as_named_str("Class name", class_name_v, &err);
			or_quit_with(NULL);

		if (sepv_is_obj(sepv))
			return sepstr_sprintf("<%s at %llx>", sepstr_to_cstr(class_name), (uint64_t)(intptr_t)sepv_to_obj(sepv));
		else
			return sepstr_sprintf("<%s object>", sepstr_to_cstr(class_name));
	} else {
		// this must be an object, primitives all have <class>
		return sepstr_sprintf("<classless object at %llx>", (uint64_t)(intptr_t)sepv_to_obj(sepv));
	}
}
