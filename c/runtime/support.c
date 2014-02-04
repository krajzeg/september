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
