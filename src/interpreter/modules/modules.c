/*****************************************************************
 **
 ** modules/modules.c
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

#include <stdlib.h>
#include <septvm.h>
#include "../platform/platform.h"

// ===============================================================
//  Loading a SharedObject
// ===============================================================

ModuleNativeCode *load_native_code(SharedObject *object) {
	ModuleNativeCode *mnc = malloc(sizeof(ModuleNativeCode));

	mnc->before_bytecode = shared_get_function(object, "module_initialize_early");
	mnc->after_bytecode = shared_get_function(object, "module_initialize_late");

	return mnc;
}
