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
//  Loading a SharedObject
// ===============================================================

// Creates an array of paths which will be searched for .dll, .so,
// .09 and .sep files that represent September modules.
SepArray *module_search_paths() {
	SepArray *paths = array_create(3);
	array_push(paths, sepv_string("."));
	array_push(paths, sepv_string("./modules"));

	SepString *interpreter_dir = get_executable_path();
	SepString *intp_modules_dir = sepstr_sprintf("%s/modules",
			sepstr_to_cstr(interpreter_dir));
	array_push(paths, str_to_sepv(intp_modules_dir));

	return paths;
}

// Finds a named file looking in all the module search paths.
SepString *find_file(SepArray *search_paths, SepString *filename, SepError *out_err) {
	SepArrayIterator iterator = array_iterate_over(search_paths);
	while (!arrayit_end(&iterator)) {
		// build path
		SepString *search_path = sepv_to_str(arrayit_next(&iterator));
		SepString *file_path = sepstr_sprintf("%s/%s",
				sepstr_to_cstr(search_path), sepstr_to_cstr(filename));

		// return the file if it exists at all
		if (file_exists(sepstr_to_cstr(file_path)))
			return file_path;
	}

	// nothing found
	return NULL;
}

// Extracts the native functions used in module initialization
// from an open shared object.
ModuleNativeCode *load_native_code(SharedObject *object) {
	ModuleNativeCode *mnc = malloc(sizeof(ModuleNativeCode));

	mnc->before_bytecode = shared_get_function(object, "module_initialize_early");
	mnc->after_bytecode = shared_get_function(object, "module_initialize_late");

	return mnc;
}

// Takes a fully qualified module name, finds all the files that
// could make up part of it (.sep, .09, .so, .dll) and makes a
// module definition ready to be loaded based on those files.
ModuleDefinition *find_module(SepString *module_name, SepError *out_err) {
	SepError err = NO_ERROR;
	SepArray *search_paths = module_search_paths();

	// any shared objects?
	ModuleNativeCode *native_code = NULL;
	SepString *shared_object_name = sepstr_sprintf("%s.sept.dll", sepstr_to_cstr(module_name));
	SepString *shared_object_path = find_file(search_paths, shared_object_name, &err);
		or_handle(EAny) { shared_object_path = NULL; }
	if (shared_object_path) {
		SharedObject *object = shared_open(sepstr_to_cstr(shared_object_path), &err);
			or_quit_with(NULL);
		native_code = load_native_code(object);
	}

	// TODO: also find .sep and .09 files

	// create module definition
	if (!native_code) {
		fail(NULL, e_module_not_found(sepstr_to_cstr(module_name)));
	}

	return moduledef_create(NULL, native_code);
}


