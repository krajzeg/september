#ifndef _SEP_ERRORS_H
#define _SEP_ERRORS_H

/*****************************************************************
 **
 ** common/errors.h
 **
 ** Anything to do with error handling goes here. This file defines
 ** the set of possible errors, as well as declaring the error
 ** structure itself and helper macros for handling errors
 ** efficiently.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
// Includes
// ===============================================================

#include <stdbool.h>

// ===============================================================
// Error types
// ===============================================================

/**
 Each error code is structured with a category and a code within
 the category, for example EFileNotFound is 0x0101 - category 0x01
 (EFile, file related errors), code 0x01.

 The upper two bytes are a mask used for matching. Category codes
 like EFile have a 0xFF00 mask, which means that when using
 error_matches(EFile), only the category byte will matter. This
 allows us to easily write code that handles all errors from
 a given category.
**/
typedef enum SepErrorType {
    // a special code, matches any error (since the mask is 0x0000)
	EAny  = 0x00000000,

    // no error
    ENone = 0x00000000,

	// file related errors
	EFile = 0xFF000100,
	    // a required file was not found
		EFileNotFound = 0xFFFF0101,
		// the file being read was not a valid september module (wrong header)
		ENotSeptemberFile = 0xFFFF0102,
		// encountered EOF while still expecting more of the file
		EUnexpectedEOF = 0xFFFF0103,
		// a catch-all error for any corruption of the module file being read
		EMalformedModuleFile = 0xFFFF0104,

	// fatal runtime errors
	EFatal = 0xFF000200,
		// running out of memory for objects at runtime
		EOutOfMemory = 0xFFFF0201,
		// missing implementation in the interpreter
		ENotImplementedYet = 0xFFFF0202

} SepErrorType;

// ===============================================================
// Error struct
// ===============================================================

/**
 Represents errors arising during execution. The structure allow for chaining
 errors (exception-like "error A caused error B").
**/
typedef struct SepError {
    // what error type this is
	SepErrorType type;
	// true if this error was already explicitly handled
	bool handled;
	// the message that explains the error to the end user
	char* message;
	// the error which caused this error to occured
	// or NULL if this is the root cause
	struct SepError* previous;
} SepError;

// ===============================================================
// Functions for working with errors
// ===============================================================

// Checks whether 'error' matches the given 'type' - if 'type' is a concrete
// error like EFileNotFound, it will only match this error. If 'type' is a
// a category (like EFile) or EAny, it will match the specified group of
// errors.
bool error_matches(SepError error, SepErrorType type);
// Frees the memory used up by this error's message and any sub-errors.
void error_free(SepError error);
// Reports this error to the user through stderr.
void error_report(SepError error);

// ===============================================================
// Functions for quickly creating errors of a given type
// ===============================================================

/**
  See 'Error types' above for more information about each error.
  Each of those functions creates a complete error object for
  a given type.
**/
SepError e_file_not_found(const char *filename); 
SepError e_not_september_file();
SepError e_unexpected_eof(); 
SepError e_malformed_module_file(const char *detail);

SepError e_out_of_memory();
SepError e_not_implemented_yet(const char *what);

// ===============================================================
// Macros for easier error handling
// ===============================================================

#define or_quit() ; if (err.type && (!err.handled)) { *out_err = err; return; };
#define or_quit_with(rv) ; if (err.type && (!err.handled)) { *out_err = err; return rv; };
#define or_go(label) ; if (err.type && (!err.handled)) { *out_err = err; goto label; };
#define or_handle(error_type) ; if (error_matches(err, error_type) && (err.handled = true))

#define _fail1(error) do { *out_err = error; return; } while(0);
#define _fail2(rv,error) do { *out_err = error; return rv; } while(0);
#define _fail_macro_name(_2, _1, name, ...) name
#define fail(...) _fail_macro_name(__VA_ARGS__, _fail2, _fail1)(__VA_ARGS__)

#define error_clear(error) do { error_free(error); error.type = 0; } while(0)
#define NO_ERROR {ENone, false, NULL, NULL}

/*****************************************************************/

#endif
