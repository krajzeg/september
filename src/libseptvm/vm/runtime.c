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

RuntimeObjects rt = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
BuiltinExceptions exc = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

#define store(into, property_name) do { into->property_name = prop_as_obj(globals_v, #property_name, &err); or_raise_sepv(err) } while(0)
SepV initialize_runtime_references(RuntimeObjects *rt, BuiltinExceptions *exc, SepV globals_v) {
	SepV err = SEPV_NOTHING;

	// - store references to various often-used objects to allow for easy access

	// object prototype
	store(rt, Object);

	// globals and syntax
	store(rt, globals);
	store(rt, syntax);

	// built-in exception types
	store(exc, Exception);
	store(exc, EWrongType);
	store(exc, EWrongIndex);
	store(exc, EWrongArguments);
	store(exc, EMissingProperty);
	store(exc, EPropertyAlreadyExists);
	store(exc, ECannotAssign);
	store(exc, ENumericOverflow);
	store(exc, EInternal);

	store(exc, ENoMoreElements);
	store(exc, EBreak);
	store(exc, EContinue);

	store(exc, EMissingModule);
	store(exc, EMalformedModule);

	store(exc, EFile);

	// other primitives and built-ins
	store(rt, Array);
	store(rt, Bool);
	store(rt, Integer);
	store(rt, NothingType);
	store(rt, String);
	store(rt, Function);
	store(rt, Slot);

	rt->Cls = prop_as_obj(globals_v, "Class", &err);
		or_raise_sepv(err);

	return SEPV_NOTHING;
}
#undef store
