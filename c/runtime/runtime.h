#ifndef _SEP_RUNTIME_H
#define _SEP_RUNTIME_H

// ===============================================================
//  Includes
// ===============================================================

#include "../vm/objects.h"

// ===============================================================
// Built-in classes (prototype objects)
// ===============================================================

extern SepObj *obj_Globals;
extern SepObj *obj_Syntax;

extern SepObj *proto_Object;
extern SepObj *proto_Array;

extern SepObj *proto_Integer;
extern SepObj *proto_String;
extern SepObj *proto_Bool;

extern SepObj *proto_Nothing;

extern SepObj *proto_Exceptions;

// ===============================================================
//  Method to actually initialize the thing
// ===============================================================

void initialize_runtime();

#endif
