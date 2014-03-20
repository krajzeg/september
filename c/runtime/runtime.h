#ifndef _SEP_RUNTIME_H
#define _SEP_RUNTIME_H

// ===============================================================
//  Includes
// ===============================================================

#include "../vm/objects.h"

// ===============================================================
// Built-in classes (prototype objects)
// ===============================================================

typedef struct RuntimeObjects {
  SepObj *globals;
  SepObj *syntax;

  SepObj *Object;
  SepObj *Array;
  SepObj *Integer;
  SepObj *String;
  SepObj *Bool;
  SepObj *NothingType;

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

void initialize_runtime();

#endif
