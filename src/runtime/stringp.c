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

// ===============================================================
// Sequence interface
// ===============================================================

SepV verify_index(SepString *this, SepInt index, bool open) {
	SepInt max = open ? (this->length + 1) : (this->length);
	if ((index < 0) || (index >= max)) {
		raise_sepv(exc.EWrongIndex, "Index '%d' is out of bounds.", index);
	} else {
		return SEPV_NOTHING;
	}
}

SepItem string_at(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepString *this = target_as_str(scope, &err); or_raise(err);
	SepInt index = param_as_int(scope, "index", &err); or_raise(err);
	or_raise(verify_index(this, index, false));

	SepString *character = sepstr_sprintf("%c", this->cstr[index]);
	return item_rvalue(str_to_sepv(character));
}

SepItem string_slice(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepString *this = target_as_str(scope, &err); or_raise(err);
	SepInt from = param_as_int(scope, "from", &err); or_raise(err);
	SepInt to = param_as_int(scope, "to", &err); or_raise(err);
	or_raise(verify_index(this, from, true)); or_raise(verify_index(this, to, true));

	SepInt len = to - from;
	if (len < 0) len = 0;

	char *buffer = mem_unmanaged_allocate(to - from + 1);
	strncpy(buffer, this->cstr + from, to-from);
	buffer[len] = '\0';
	SepString *sliced = sepstr_new(buffer);
	mem_unmanaged_free(buffer);

	return item_rvalue(str_to_sepv(sliced));
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

	// === sequence methods
	obj_add_builtin_method(String, "at", &string_at, 1, "index");
	obj_add_builtin_method(String, "slice", &string_slice, 2, "from", "to");
	obj_add_builtin_method(String, "length", &string_length, 0);

	// === string methods
	obj_add_builtin_method(String, "upperCase", &string_upper, 0);

	// === operators
	obj_add_builtin_method(String, "+", &string_plus, 1, "other");
	obj_add_builtin_method(String, "==", &string_equals, 1, "other");
	obj_add_builtin_method(String, "<compareTo>", &string_compare, 1, "other");

	return String;
}
