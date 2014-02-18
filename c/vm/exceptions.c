/*****************************************************************
 **
 ** vm/exceptions.c
 **
 ** Implementation for exceptions.h
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "types.h"
#include "objects.h"
#include "exceptions.h"
#include "../runtime/support.h"

// ===============================================================
//  Working with exceptions
// ===============================================================

// Creates a new live exception and returns it as a SepV.
SepV sepv_exception(SepObj *prototype, SepString *message) {
	// create a simple exception object
	if (prototype == NULL)
		prototype = builtin_exception("Exception");
	SepObj *exception_obj = obj_create_with_proto(obj_to_sepv(prototype));

	// set the message
	props_add_field(exception_obj, "message", str_to_sepv(message));

	// return it
	return obj_to_exception(exception_obj);
}

// Creates a new live exception and returns it as an r-value SepItem.
SepItem si_exception(SepObj *prototype, SepString *message) {
	return item_rvalue(sepv_exception(prototype, message));
}
