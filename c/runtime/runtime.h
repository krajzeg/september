#ifndef _SEP_RUNTIME_H
#define _SEP_RUNTIME_H

// ===============================================================
//  Includes
// ===============================================================

#include "../vm/objects.h"

// ===============================================================
// Built-in classes (prototype objects)
// ===============================================================

extern SepObj *proto_Object;
extern SepObj *proto_String;
extern SepObj *proto_Integer;

// ===============================================================
//  Method to actually initialize the thing
// ===============================================================

void initialize_runtime(SepObj *scope);

#endif
