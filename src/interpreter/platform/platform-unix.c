/*****************************************************************
 **
 ** platform/platform-unix.c
 **
 ** A Unix-compatible implementation for functions from platform/platform.h
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "platform.h"

// ===============================================================
//  Paths
// ===============================================================

// Tests for existence of a file (wrapper for stat()).
bool file_exists(const char *path) {
	// the stat() based implementation returns FALSE also
	// when the file is inaccessible, but that's OK for now
	struct stat stat_buffer;
	int stat_result = stat(path, &stat_buffer);
	return stat_result == 0;
}

// Creates an array of paths which will be searched for .dll, .so,
// .09 and .sep files that represent September modules.
SepArray *module_search_paths() {
	static SepArray *paths = NULL;

	// already initialized?
	if (paths) return paths;

	// initialize array
	paths = array_create(3);
	array_push(paths, sepv_string("."));
	array_push(paths, sepv_string("./modules"));

	return paths;
}

// ===============================================================
//  Shared objects
// ===============================================================

typedef struct UnixSharedObject {
	void *handle;
} UnixSharedObject;

// Opens a new shared object store under the path provided.
SharedObject *shared_open(const char *path, SepError *out_err) {
	// load the .so
	void *handle = dlopen(path, RTLD_LAZY);

	// allocate and return a result
	UnixSharedObject *so = malloc(sizeof(UnixSharedObject));
	so->handle = handle;
	return (SharedObject*)so;
}

// Returns the name of the expected shared object file corresponding to
// the module name.
SepString *shared_filename(SepString *module_name) {
	return sepstr_sprintf("%s.sept.so", sepstr_to_cstr(module_name));
}

// Retrieves a function from an open shared object.
void *shared_get_function(SharedObject *object, const char *name) {
	void *handle = ((UnixSharedObject*)object)->handle;
	return dlsym(handle, name);
}

// Closes a previously opened shared object.
void shared_close(SharedObject *object) {
	UnixSharedObject *uso = (UnixSharedObject*)object;
	dlclose(uso->handle);
	free(uso);
}
