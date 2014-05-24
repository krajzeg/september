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

// Takes a fully qualified module name, finds all the files that
// could make up part of it (.sep, .09, .so, .dll) and makes a
// module definition ready to be loaded based on those files.
ModuleDefinition *find_module_files(SepString *module_name, SepV *error);

#endif
