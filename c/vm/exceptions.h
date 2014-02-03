#ifndef _SEP_EXCEPTIONS_H_
#define _SEP_EXCEPTIONS_H_

/*****************************************************************
 **
 ** vm/exceptions.h
 **
 ** Contains everything dealing with exceptions and how they are
 ** handled in the September VM.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "types.h"
#include "objects.h"

// ===============================================================
//  Working with exceptions
// ===============================================================

// Creates a new live exception and returns it as a SepV.
SepV sepv_exception(SepObj *prototype, SepString *message);
// Creates a new live exception and returns it as an r-value.
SepItem si_exception(SepObj *prototype, SepString *message);

// ===============================================================
//  Conversion macros
// ===============================================================

#define obj_to_exception(obj) (SEPV_TYPE_EXCEPTION | (((intptr_t)obj) >> 3))
#define exception_to_obj(val) ((SepObj*)(((intptr_t)val) << 3))
#define exception_to_obj_sepv(val) (((val) & (~SEPV_TYPE_MASK)) | SEPV_TYPE_OBJECT)
#define obj_sepv_to_exception(obj) (((val) & (~SEPV_TYPE_MASK)) | SEPV_TYPE_EXCEPTION)
#define sepv_is_exception(val) ((val & SEPV_TYPE_MASK) == SEPV_TYPE_EXCEPTION)

/*****************************************************************/

#endif
