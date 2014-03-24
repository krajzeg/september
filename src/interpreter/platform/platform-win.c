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
//  Paths
// ===============================================================

// Returns the path to the directory where the interpreter resides.
// The returned string has to be freed by the caller.
SepString *get_executable_path() {
	// extract executable filename
	char *buffer = malloc(MAX_PATH + 1);
	memset(buffer, 0, MAX_PATH + 1);
	GetModuleFileName(NULL, buffer, MAX_PATH);

	// truncate at last slash to get just the path
	char *last_slash = strrchr(buffer, '\\');
	*last_slash = '\0';

	// return as SepString*
	return sepstr_create(buffer);
}

// Tests for existence of a file (wrapper for stat()).
bool file_exists(const char *path) {
	// the stat() based implementation returns FALSE also
	// when the file is inaccessible, but that's OK for now
	struct stat stat_buffer;
	int stat_result = stat(path, &stat_buffer);
	return stat_result == 0;
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
	WindowsSharedObject *so = malloc(sizeof(WindowsSharedObject));
	so->handle = handle;
	return (SharedObject*)so;
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
	free(wso);
}
