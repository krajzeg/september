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

#include "../vm/stack.h"
#include "../vm/types.h"
#include "../vm/objects.h"
#include "../vm/functions.h"
#include "../vm/vm.h"
#include "runtime.h"
#include "support.h"

// ===============================================================
// Methods
// ===============================================================

SepItem string_upper(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;
	SepString *this = target_as_str(scope, &err); or_raise(NULL);

	SepString *upper_str = mem_allocate(sepstr_allocation_size(this->content));
	upper_str->length = this->length;
	char *src = this->content;
	char *dest = upper_str->content;
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
	SepObj *String = obj_create();

	BuiltInFunc *upper = builtin_create(&string_upper, 0);
	props_accept_prop(String, sepstr_create("upper"), method_create(func_to_sepv(upper)));

	return String;
}
