#ifndef _SEP_RUNTIME_H
#define _SEP_RUNTIME_H

// ===============================================================
//  Built-in classes (prototype objects)
// ===============================================================

extern SepObj *proto_Object;
extern SepObj *proto_String;

// ===============================================================
//  Method to actually initialize the thing
// ===============================================================

void introduce_builtins(SepObj *scope);

#endif
