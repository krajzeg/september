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
#include "platform.h"

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
