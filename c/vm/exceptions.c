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

// ===============================================================
//  Working with exceptions
// ===============================================================

// Creates a new live exception and returns it as a SepV.
SepV sepv_exception(SepObj *prototype, SepString *message) {
	// create a simple exception object
	SepObj *exception_obj = obj_create();
	exception_obj->prototypes = obj_to_sepv(prototype);
	props_add_field(exception_obj, "message", str_to_sepv(message));

	// return it
	return obj_to_exception(exception_obj);
}

// Creates a new live exception and returns it as a SepV.
SepItem si_exception(SepObj *prototype, SepString *message) {
	// create a simple exception object
	SepObj *exception_obj = obj_create();
	exception_obj->prototypes = obj_to_sepv(prototype);
	props_add_field(exception_obj, "message", str_to_sepv(message));

	// return it
	return item_rvalue(obj_to_exception(exception_obj));
}
