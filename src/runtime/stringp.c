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

SepItem string_view(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;

	// extract arguments
	SepString *this = target_as_str(scope, &err); or_raise(err);
	SepV char_seq = param(scope, "indices");

	// create a SepString of necessary length
	SepV length_v = call_method(frame->vm, char_seq, "length", 0); or_raise(length_v);
	SepInt length = cast_as_int(length_v, &err); or_raise(err);
	SepString *result = sepstr_with_length(length);

	// iterate over the index sequence
	SepV iterator = call_method(frame->vm, char_seq, "iterator", 0); or_raise(iterator);
	SepV iterator_next = property(iterator, "next"); or_raise(iterator_next);

	int position = 0;
	while (true) {
		SepV element = vm_invoke(frame->vm, iterator_next, 0).value;

		// break on ENoMoreElements
		SepV enomoreelements_v = obj_to_sepv(exc.ENoMoreElements);
		SepV is_no_more_elements_v = call_method(frame->vm, element, "is",
				1, enomoreelements_v);
		or_raise(is_no_more_elements_v);
		if (is_no_more_elements_v == SEPV_TRUE) {
			break;
		}

		// any other exception is propagated
		or_raise(element);

		// append character
		SepInt index = cast_as_int(element, &err); or_raise(err);
		result->cstr[position++] = this->cstr[index];
	}

	return item_rvalue(str_to_sepv(result));
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
	obj_add_builtin_method(String, "view", &string_view, 1, "indices");
	obj_add_builtin_method(String, "length", &string_length, 0);

	// === string methods
	obj_add_builtin_method(String, "upperCase", &string_upper, 0);

	// === operators
	obj_add_builtin_method(String, "+", &string_plus, 1, "other");
	obj_add_builtin_method(String, "==", &string_equals, 1, "other");
	obj_add_builtin_method(String, "<compareTo>", &string_compare, 1, "other");

	return String;
}
