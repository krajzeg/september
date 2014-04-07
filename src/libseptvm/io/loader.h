#ifndef LOADER_H_
#define LOADER_H_

/*****************************************************************
 **
 ** loader.h
 **
 ** Top-level API for loading modules into the virtual machine.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdint.h>
#include <stdio.h>
#include "../vm/objects.h"
#include "../vm/module.h"
#include "../vm/vm.h"
#include "../common/errors.h"

// ===============================================================
//  Module definitions
// ===============================================================

/**
 * Byte sources are used as sources from which bytecode is loaded.
 * The bytes might come from a file or from the output of a parser.
 */
struct ByteSourceVT;
typedef struct ByteSource {
	struct ByteSourceVT *vt;
} ByteSource;

typedef struct ByteSourceVT {
	uint8_t (*get_next_byte)(ByteSource *this, SepError *out_err);
	void (*close_and_free)(ByteSource *this);
} ByteSourceVT;

/**
 * Native components of the module, stored in .dll/.so files, are
 * parsed into a ModuleNativeCode structure. The VM initialization
 * function is used to synchronize the September runtime between
 * the DLL and main process (to make sure e.g. 'Array' means the
 * same thing in both). The 'before/after_bytecode' methods can
 * be used to add native functionality to objects in the module -
 * before or after the part of the module written September is
 * executed.
 */
typedef void (*VMInitFunc)(LibSeptVMGlobals *globals, SepError *out_err);
typedef void (*ModuleInitFunc)(SepModule *module, SepError *out_err);
typedef struct ModuleNativeCode {
	VMInitFunc initialize_slave_vm;
	ModuleInitFunc early_initializer;
	ModuleInitFunc late_initializer;
} ModuleNativeCode;


/**
 * Encapsulates everything needed to properly load and initialize
 * a module.
 */
typedef struct ModuleDefinition {
	// the name of this module
	SepString *name;
	// the bytecode for the module, or NULL if there isn't any
	ByteSource *bytecode;
	// the native code for the module, or NULL if there isn't any
	ModuleNativeCode *native;
} ModuleDefinition;

// Creates a new module definition from its components.
ModuleDefinition *moduledef_create(ByteSource *bytecode, ModuleNativeCode *native);
// Destroys a module definition and frees its components.
void moduledef_free(ModuleDefinition *this);

// ===============================================================
//  File sources
// ===============================================================

// Creates a byte source pulling from a provided file.
ByteSource *file_bytesource_create(const char *filename, SepError *out_err);

// ===============================================================
//  Loading
// ===============================================================

// Module finder function to be provided by the interpreter.
typedef ModuleDefinition *(*ModuleFinderFunc)(SepString *name, SepError *out_err);

// Must be executed by the interpreter to let the loader library know how to find
// modules.
void initialize_module_loader(ModuleFinderFunc find_module);
// Loads the module from a given definition and returns the module root object.
// If anything goes wrong, returns an exception instead.
SepV load_module(ModuleDefinition *definition);
// Loads a module by its string name. Uses functionality delivered by the interpreter
// to find the module. If anything goes wrong, returns an exception instead.
SepV load_module_by_name(SepString *module_name);

/*****************************************************************/

#endif
