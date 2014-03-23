#ifndef INTP_MODULES_H_
#define INTP_MODULES_H_

/*****************************************************************
 **
 ** modules/modules.h
 **
 ** All interpreter functionalities pertaining to module lookup or
 ** module loading.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <septvm.h>
#include "../platform/platform.h"

// ===============================================================
//  Shared object loading
// ===============================================================

// Extracts the native functions used in module initialization
// from an open shared object.
ModuleNativeCode *load_native_code(SharedObject *object);

#endif
