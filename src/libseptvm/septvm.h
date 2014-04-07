#ifndef SEPTVM_H_
#define SEPTVM_H_

/*****************************************************************
 **
 ** septvm.h
 **
 ** Public header for libseptvm, includes all other headers that
 ** contain something of public interest.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Public include list
// ===============================================================

#include "common/debugging.h"
#include "common/errors.h"
#include "common/garray.h"
#include "io/loader.h"
#include "vm/runtime.h"
#include "vm/support.h"
#include "vm/arrays.h"
#include "vm/exceptions.h"
#include "vm/functions.h"
#include "vm/mem.h"
#include "vm/gc.h"
#include "vm/module.h"
#include "vm/objects.h"
#include "vm/types.h"
#include "vm/vm.h"

// ===============================================================
//  Macros for modules
// ===============================================================

#ifdef _WIN32
  #define MODULE_EXPORT __declspec(dllexport)
#else
  #define MODULE_EXPORT
#endif

/*****************************************************************/

#endif
