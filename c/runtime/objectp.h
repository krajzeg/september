#ifndef _SEP_RT_OBJECT_H
#define _SEP_RT_OBJECT_H

/*****************************************************************
 **
 ** runtime/objectp.h
 **
 ** Implementation for the Object prototype visible from
 ** inside September. Methods accessible to all September objects
 ** are implemented here.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "../vm/objects.h"

// ===============================================================
//  String prototype
// ===============================================================

// Creates the Object class object.
SepObj *create_object_prototype();

/*****************************************************************/

#endif
