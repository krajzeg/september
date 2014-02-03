#ifndef _SEP_RT_STRING_H
#define _SEP_RT_STRING_H_

/*****************************************************************
 **
 ** runtime/stringp.h
 **
 ** Implementation for the String prototype object visible from
 ** inside September. All methods of class String are implemented
 ** here.
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

// Creates the String class object.
SepObj *create_string_prototype();

/*****************************************************************/

#endif
