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

/*****************************************************************/

#endif
