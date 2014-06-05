/*****************************************************************
 **
 ** runtime/stringp.c
 **
 ** Implementation for all built-in String methods.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <string.h>
#include "common.h"

// ===============================================================
// Methods
// ===============================================================

SepItem string_upper(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepString *this = target_as_str(scope, &err); or_raise(err);

	SepString *upper_str = sepstr_new(this->cstr);
	char *src = this->cstr;
	char *dest = upper_str->cstr;
	do {
		if ((*src >= 97) && (*src <= 122))
			*(dest++) = *src - 32;
		else
			*(dest++) = *src;
	} while (*(src++));

	return item_rvalue(str_to_sepv(upper_str));
}

SepItem string_length(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepString *this = target_as_str(scope, &err); or_raise(err);
	return si_int(this->length);
}

// ===============================================================
//  Operators
// ===============================================================

SepItem string_plus(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepString *this = target_as_str(scope, &err);
		or_raise(err);
	SepString *other = param_as_str(scope, "other", &err);
		or_raise(err);

	SepString *concatenated = sepstr_sprintf("%s%s", this->cstr, other->cstr);
	return item_rvalue(str_to_sepv(concatenated));
}

SepItem string_equals(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepString *this = target_as_str(scope, &err);
		or_raise(err);
	SepString *other = param_as_str(scope, "other", &err);
		or_handle() { return si_bool(false); }
	return si_bool(sepstr_cmp(this, other) == 0);
}

SepItem string_compare(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepString *this = target_as_str(scope, &err);
		or_raise(err);
	SepString *other = param_as_str(scope, "other", &err);
		or_handle() {
			// signal uncomparable values
			return si_nothing();
		};
	return si_int(sepstr_cmp(this, other));
}

// ===============================================================
//  Creation of prototype
// ===============================================================

SepObj *create_string_prototype() {
	SepObj *String = make_class("String", NULL);

	// === methods
	obj_add_builtin_method(String, "length", &string_length, 0);
	obj_add_builtin_method(String, "upperCase", &string_upper, 0);

	// === operators
	obj_add_builtin_method(String, "+", &string_plus, 1, "other");
	obj_add_builtin_method(String, "==", &string_equals, 1, "other");
	obj_add_builtin_method(String, "<compareTo>", &string_compare, 1, "other");

	return String;
}
