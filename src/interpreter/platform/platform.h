#ifndef PLATFORM_H_
#define PLATFORM_H_

/*****************************************************************
 **
 ** platform/platform.h
 **
 ** A header for platform-dependent functions. Currently, this
 ** unifies the API for working with Windows DLLs and Unix .so
 ** files.
 **
 ***************
 ** September **
 *****************************************************************/

#include <septvm.h>

// ===============================================================
//  Paths
// ===============================================================

// Returns the path to the directory where the interpreter resides.
SepString *get_executable_path();
// Tests for existence of a file (wrapper for stat()).
bool file_exists(const char *path);

// ===============================================================
//  Shared objects
// ===============================================================

/**
 * Each platform will use its own struct, so this is just an empty
 * supertype for all of them.
 */
typedef struct SharedObject {
} SharedObject;

// Opens a new shared object store under the path provided.
SharedObject *shared_open(const char *path, SepError *out_err);
// Retrieves a function from an open shared object.
void *shared_get_function(SharedObject *object, const char *name);
// Closes a previously opened shared object.
void shared_close(SharedObject *object);

#endif
