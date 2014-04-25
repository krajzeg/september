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
//  Initialization
// ===============================================================

// Some platforms need access to argc and argv, and this function
// is used to grant them that.
void platform_initialize(int argc, char **argv);

// ===============================================================
//  Paths
// ===============================================================

// Tests for existence of a file (wrapper for stat()).
bool file_exists(const char *path);
// Creates an array of paths which will be searched for .dll, .so,
// .09 and .sept files that represent September modules.
SepArray *module_search_paths();

// ===============================================================
//  Shared objects
// ===============================================================

/**
 * Each platform will use its own struct, so this is just an empty
 * supertype for all of them.
 */
typedef struct SharedObject {
} SharedObject;

// Returns the name of the expected shared object file corresponding to
// the module name.
SepString *shared_filename(SepString *module_name);

// Opens a new shared object store under the path provided.
SharedObject *shared_open(const char *path, SepError *out_err);
// Retrieves a function from an open shared object.
void *shared_get_function(SharedObject *object, const char *name);
// Closes a previously opened shared object.
void shared_close(SharedObject *object);

#endif
