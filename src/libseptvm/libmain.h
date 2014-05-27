#ifndef LIBMAIN_H_
#define LIBMAIN_H_

/*****************************************************************
 **
 ** libmain.h
 **
 ** Hosts the global libseptvm initialization routines, as well as
 ** some global variables used throughout.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes and pre-declarations
// ===============================================================

#include <stdint.h>

struct ManagedMemory;
struct SepObj;
struct SepVM;
struct GenericArray;

// ===============================================================
//  Globals used by LibSeptVM
// ===============================================================

typedef struct LibSeptVMGlobals {
	// the managed memory used by libseptvm
	struct ManagedMemory *memory;
	// the loaded September module cache
	struct SepObj *module_cache;
	// the interned string cache
	struct SepObj *string_cache;
	// garbage collection contexts
	struct GenericArray *gc_contexts;
	// accessing the VM thread-local
	struct SepVM *(*get_vm_for_current_thread)();
	struct SepVM *(*set_vm_for_current_thread)(struct SepVM *);

	// global used for property resolution cache invalidation
	uint64_t property_cache_version;

	// names of the library modules for which debug logging is turned on
	char *debugged_module_names;
} LibSeptVMGlobals;

// all the globals used by libseptvm in one place
extern LibSeptVMGlobals lsvm_globals;

// ===============================================================
//  Initialization of the whole library
// ===============================================================

// Initializes a slave libseptvm (as used inside a module DLL/.so). This is needed
// so that things like the memory manager can be shared with the master process.
void libseptvm_initialize_slave(LibSeptVMGlobals *parent_config);
// Initializes the master libseptvm in the interpreter.
void libseptvm_initialize();

/*****************************************************************/

#endif
