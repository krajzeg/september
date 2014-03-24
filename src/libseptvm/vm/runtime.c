/*****************************************************************
 **
 ** runtime.c
 **
 **
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "../vm/support.h"
#include "runtime.h"

// ===============================================================
//  Extracting runtime objects
// ===============================================================

RuntimeObjects rt;
BuiltinExceptions exc;

#define store(into, property_name) into.property_name = prop_as_obj(globals_v, #property_name, &err);
void initialize_runtime_references(SepV globals_v) {
	SepError err = NO_ERROR;

	// - store references to various often-used objects to allow for easy access

	// globals and syntax
	store(rt, globals);
	store(rt, syntax);

	// runtime classes
	store(rt, Array);
	store(rt, Bool);
	store(rt, Integer);
	store(rt, NothingType);
	store(rt, Object);
	store(rt, String);

	// built-in exception types
	store(exc, Exception);
	store(exc, EWrongType);
	store(exc, EWrongIndex);
	store(exc, EWrongArguments);
	store(exc, EMissingProperty);
	store(exc, EPropertyAlreadyExists);
	store(exc, ECannotAssign);
	store(exc, ENoMoreElements);
	store(exc, ENumericOverflow);
	store(exc, EInternal);
}
#undef store