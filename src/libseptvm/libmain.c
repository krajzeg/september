/*****************************************************************
 **
 ** libmain.c
 **
 ** The global libseptvm initialization routines, as well as
 ** the place where lsvm_globals is instantiated.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "vm/vm.h"
#include "vm/gc.h"
#include "vm/runtime.h"
#include "libmain.h"

// ===============================================================
//  Globals
// ===============================================================

// All the globals under one roof
LibSeptVMGlobals lsvm_globals = {NULL, NULL, NULL, NULL, NULL};

// While a VM is running, a pointer to it is always stored here. Every thread can
// only run one VM at a time, and each thread has its own VM.
__thread SepVM *_currently_running_vm = NULL;

// ===============================================================
//  Internals
// ===============================================================

/* Functions for access to the thread-local (access through a function guarantees
   thread-locals working properly across DLL boundaries) */
SepVM *_get_vm_for_current_thread() {
	return _currently_running_vm;
}

SepVM *_set_vm_for_current_thread(SepVM *new_vm) {
	SepVM *previous = _currently_running_vm;
	_currently_running_vm = new_vm;
	return previous;
}

// ===============================================================
//  Initialization routines
// ===============================================================

// Initializes a slave libseptvm (as used inside a module DLL/.so). This is needed
// so that things like the memory manager can be shared with the master process.
void libseptvm_initialize_slave(LibSeptVMGlobals *parent_config) {
	lsvm_globals = *parent_config;
}

// Initializes the master libseptvm in the interpreter.
void libseptvm_initialize() {
	lsvm_globals.get_vm_for_current_thread = &_get_vm_for_current_thread;
	lsvm_globals.set_vm_for_current_thread = &_set_vm_for_current_thread;

	lsvm_globals.memory = mem_initialize();
	lsvm_globals.gc_contexts = ga_create(0, sizeof(GCContext*), &allocator_unmanaged);
	lsvm_globals.debugged_module_names = mem_unmanaged_allocate(4096);
	lsvm_globals.debugged_module_names[0] = '\0';
	lsvm_globals.property_cache_version = 0;
	lsvm_globals.runtime_objects = &rt;
	lsvm_globals.builtin_exceptions = &exc;

	gc_start_context();
	lsvm_globals.module_cache = obj_create_with_proto(SEPV_NOTHING);
	lsvm_globals.string_cache = obj_create_with_proto(SEPV_NOTHING);
	gc_end_context();
}
