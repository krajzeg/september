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
	SepObj *Function;
	SepObj *Slot;
	SepObj *NothingType;

	// the class object
	SepObj *Cls;
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
	SepObj *EBreak;
	SepObj *EContinue;

	SepObj *ENumericOverflow;

	SepObj *EMissingModule;
	SepObj *EMalformedModule;

	SepObj *EFile;
	SepObj *EInternal;
} BuiltinExceptions;

extern RuntimeObjects rt;
extern BuiltinExceptions exc;

// ===============================================================
//  Method to actually initialize the thing
// ===============================================================

// Extracts references to commonly used objects from the provided "globals" variable.
SepV initialize_runtime_references(RuntimeObjects *rt, BuiltinExceptions *exc, SepV globals_v);

#endif
