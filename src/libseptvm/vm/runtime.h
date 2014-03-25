#ifndef _SEP_RUNTIME_H
#define _SEP_RUNTIME_H

/*****************************************************************
 **
 ** vm/runtime.h
 **
 ** VM globals that store references to commonly used objects, like
 ** the Object class itself or built-in exception types.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes and forward definitions
// ===============================================================

#include "../vm/objects.h"

// ===============================================================
// Built-in classes (prototype objects)
// ===============================================================

typedef struct RuntimeObjects {
	// the two super-globals
	SepObj *globals;
	SepObj *syntax;

	// the primitive classes
	SepObj *Object;
	SepObj *Array;
	SepObj *Integer;
	SepObj *String;
	SepObj *Bool;
	SepObj *NothingType;

	// the string cache (interned strings live as keys here)
	SepObj *string_cache;
} RuntimeObjects;

typedef struct BuiltinExceptions {
	SepObj *Exception;

	SepObj *EWrongType;
	SepObj *EWrongIndex;
	SepObj *EWrongArguments;

	SepObj *EMissingProperty;
	SepObj *EPropertyAlreadyExists;
	SepObj *ECannotAssign;

	SepObj *ENoMoreElements;

	SepObj *ENumericOverflow;

	SepObj *EInternal;
} BuiltinExceptions;

extern RuntimeObjects rt;
extern BuiltinExceptions exc;

// ===============================================================
//  Method to actually initialize the thing
// ===============================================================

// Extracts references to commonly used objects from the provided "globals" variable.
void initialize_runtime_references(SepV globals_v);

#endif
