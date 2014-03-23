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
#include <windows.h>
#undef NO_ERROR
#include "platform.h"

// ===============================================================
//  Paths
// ===============================================================

// Returns the path to the directory where the interpreter resides.
// The returned string has to be freed by the caller.
SepString *get_executable_path() {
	char *buffer = malloc(MAX_PATH + 1);
	memset(buffer, 0, MAX_PATH + 1);
	GetModuleFileName(NULL, buffer, MAX_PATH);
	return sepstr_create(buffer);
}

// ===============================================================
//  Shared objects
// ===============================================================

typedef struct WindowsSharedObject {
	HMODULE handle;
} WindowsSharedObject;

// Opens a new shared object store under the path provided.
SharedObject *shared_open(const char *path) {
	// load the DLL
	HMODULE handle = LoadLibraryA(path);

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
