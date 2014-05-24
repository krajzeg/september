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
#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "platform.h"

// ===============================================================
//  Initialization
// ===============================================================

char **_stored_argv;

// Some platforms need access to argc and argv, and this function
// is used to grant them that.
void platform_initialize(int argc, char **argv) {
	_stored_argv = argv;
}

// ===============================================================
//  Paths
// ===============================================================

// Tries to figure out the executable path, returns NULL if it cannot.
// argv[0] is used as the only portable way on *xes.
SepString *get_executable_path() {
	char *exec_arg = _stored_argv[0];
	char *last_slash = strrchr(exec_arg, '/');
	if (last_slash == NULL)
		return NULL; // not recognized as a path
	
	// create the SepString
	char *buffer = mem_unmanaged_allocate(strlen(exec_arg) + 1);
	strncpy(buffer, exec_arg, last_slash - exec_arg);
	buffer[last_slash-exec_arg] = '\0';
	SepString *exec_path = sepstr_for(buffer);
	mem_unmanaged_free(buffer);
	
	return exec_path;
}

// Tests for existence of a file (wrapper for stat()).
bool file_exists(const char *path) {
	// the stat() based implementation returns FALSE also
	// when the file is inaccessible, but that's OK for now
	struct stat stat_buffer;
	int stat_result = stat(path, &stat_buffer);
	return stat_result == 0;
}

// Creates an array of paths which will be searched for .dll, .so,
// .09 and .sept files that represent September modules.
SepArray *module_search_paths() {
	static SepArray *paths = NULL;

	// already initialized?
	if (paths) return paths;

	// initialize array
	paths = array_create(3);
	array_push(paths, sepv_string("."));
	array_push(paths, sepv_string("./modules"));
	
	// try the executable path
	SepString *exec_path = get_executable_path();
	if (exec_path) {
		SepString *modules_dir = sepstr_sprintf("%s/modules", exec_path->cstr);
		array_push(paths, str_to_sepv(modules_dir));
	}
	
	return paths;
}

// ===============================================================
//  Shared objects
// ===============================================================

typedef struct UnixSharedObject {
	void *handle;
} UnixSharedObject;

// Opens a new shared object store under the path provided.
SharedObject *shared_open(const char *path, SepV *error) {
	// load the .so
	void *handle = dlopen(path, RTLD_LAZY);

	// allocate and return a result
	UnixSharedObject *so = mem_unmanaged_allocate(sizeof(UnixSharedObject));
	so->handle = handle;
	return (SharedObject*)so;
}

// Returns the name of the expected shared object file corresponding to
// the module name.
SepString *shared_filename(SepString *module_name) {
	return sepstr_sprintf("%s.sept.so", module_name->cstr);
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
	mem_unmanaged_free(uso);
}
