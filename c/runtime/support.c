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
#include "../common/errors.h"
#include "../vm/exceptions.h"
#include "../vm/types.h"
#include "../vm/functions.h"
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
