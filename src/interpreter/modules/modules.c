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
#include <sys/stat.h>
#include <septvm.h>
#include "../platform/platform.h"

// ===============================================================
//  Constants
// ===============================================================

char INIT_SLAVE_VM_NAME[] = "module_initialize_slave_vm";
char EARLY_INIT_NAME[] = "module_initialize_early";
char LATE_INIT_NAME[]  = "module_initialize_late";

// ===============================================================
//  Loading a SharedObject
// ===============================================================

// Finds a named file looking in all the module search paths.
SepString *find_file(SepArray *search_paths, SepString *filename) {
	SepArrayIterator iterator = array_iterate_over(search_paths);
	while (!arrayit_end(&iterator)) {
		// build path
		SepString *search_path = sepv_to_str(arrayit_next(&iterator));
		SepString *file_path = sepstr_sprintf("%s/%s",
				search_path->cstr, filename->cstr);
		
		// return the file if it exists at all
		if (file_exists(file_path->cstr))
			return file_path;
	}

	// nothing found
	return NULL;
}

// Extracts the native functions used in module initialization
// from an open shared object.
ModuleNativeCode *load_native_code(SharedObject *object) {
	ModuleNativeCode *mnc = mem_unmanaged_allocate(sizeof(ModuleNativeCode));

	mnc->initialize_slave_vm = shared_get_function(object, INIT_SLAVE_VM_NAME);
	mnc->early_initializer = shared_get_function(object, EARLY_INIT_NAME);
	mnc->late_initializer = shared_get_function(object, LATE_INIT_NAME);

	return mnc;
}

// Takes a fully qualified module name, finds all the files that
// could make up part of it (.sept, .09, .so, .dll) and makes a
// module definition ready to be loaded based on those files.
ModuleDefinition *find_module_files(SepString *module_name, SepV *error) {
	SepV err = SEPV_NOTHING;
	SepArray *search_paths = module_search_paths();

	// any shared objects?
	ModuleNativeCode *native_code = NULL;
	SepString *shared_object_filename = shared_filename(module_name);
	SepString *shared_object_path = find_file(search_paths, shared_object_filename);
	if (shared_object_path) {
		SharedObject *object = shared_open(shared_object_path->cstr, &err);
			or_fail_with(NULL);
		native_code = load_native_code(object);
	}

	// any .sept files with already compiled bytecode?
	SepString *bytecode_filename = sepstr_sprintf("%s.sept", module_name->cstr);
	SepString *bytecode_file_path = find_file(search_paths, bytecode_filename);
	ByteSource *bytecode_source = NULL;
	if (bytecode_file_path) {
		bytecode_source = file_bytesource_create(bytecode_file_path->cstr, &err);
			or_fail_with(NULL);
	}

	// TODO: also find and compile .09 files

	// create module definition
	if (!native_code && !bytecode_source) {
		fail(NULL, exception(exc.EMissingModule, "Module not found: %s.", module_name->cstr));
	}

	return moduledef_create(bytecode_source, native_code);
}
