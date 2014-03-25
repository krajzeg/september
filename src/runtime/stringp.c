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

#include <septvm.h>

// ===============================================================
// Methods
// ===============================================================

SepItem string_upper(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;
	SepString *this = target_as_str(scope, &err); or_raise(exc.EWrongType);

	SepString *upper_str = mem_allocate(sepstr_allocation_size(this->cstr));
	upper_str->length = this->length;
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
//  Creation of prototype
// ===============================================================

SepObj *create_string_prototype() {
	SepObj *String = make_class("String", NULL);

	BuiltInFunc *upper = builtin_create(&string_upper, 0);
	props_accept_prop(String, sepstr_for("upper"), method_create(func_to_sepv(upper)));

	return String;
}
