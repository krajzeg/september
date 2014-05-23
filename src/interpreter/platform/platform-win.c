/*****************************************************************
 **
 ** platform/platform-win.c
 **
 ** A Windows implementation for functions from platform/platform.h
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>
#include <sys/stat.h>
#include <windows.h>
#undef NO_ERROR
#include "platform.h"

// ===============================================================
//  Initialization
// ===============================================================

// The Windows platform does not need initialization.
void platform_initialize(int argc, char **argv) {}

// ===============================================================
//  Paths
// ===============================================================

// Returns the path to the directory where the interpreter resides.
// The returned string has to be freed by the caller.
SepString *get_executable_path() {
	// extract executable filename
	char *buffer = mem_unmanaged_allocate(MAX_PATH + 1);
	memset(buffer, 0, MAX_PATH + 1);
	GetModuleFileName(NULL, buffer, MAX_PATH);

	// truncate at last slash to get just the path
	char *last_slash = strrchr(buffer, '\\');
	*last_slash = '\0';

	// return as SepString*
	return sepstr_for(buffer);
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

	SepString *interpreter_dir = get_executable_path();
	SepString *intp_modules_dir = sepstr_sprintf("%s/modules",
			interpreter_dir->cstr);
	array_push(paths, str_to_sepv(intp_modules_dir));

	return paths;
}

// ===============================================================
//  Shared objects
// ===============================================================

typedef struct WindowsSharedObject {
	HMODULE handle;
} WindowsSharedObject;

// Opens a new shared object store under the path provided.
SharedObject *shared_open(const char *path, SepError *out_err) {
	// load the DLL
	HMODULE handle = LoadLibrary(path);

	// allocate and return a result
	WindowsSharedObject *so = mem_unmanaged_allocate(sizeof(WindowsSharedObject));
	so->handle = handle;
	return (SharedObject*)so;
}

// Returns the name of the expected shared object file corresponding to
// the module name.
SepString *shared_filename(SepString *module_name) {
	return sepstr_sprintf("%s.sept.dll", module_name->cstr);
}

// Retrieves a function from an open shared object.
void *shared_get_function(SharedObject *object, const char *name) {
	HMODULE handle = ((WindowsSharedObject*)object)->handle;
	return GetProcAddress(handle, name);
}

// Closes a previously opened shared object.
void shared_close(SharedObject *object) {
	WindowsSharedObject *wso = (WindowsSharedObject*)object;
	FreeLibrary(wso->handle);
	mem_unmanaged_free(wso);
}
