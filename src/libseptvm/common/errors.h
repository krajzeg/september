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
#include "../vm/types.h"

#define sepv_is_exception(sepv) (sepv_type(sepv) == SEPV_TYPE_EXCEPTION)

// ===============================================================
// Error struct
// ===============================================================

typedef SepV SepError;

// ===============================================================
// Functions for working with errors
// ===============================================================

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
SepError e_module_not_found(const char *module_name);
SepError e_not_september_file();
SepError e_unexpected_eof(); 
SepError e_malformed_module_file(const char *detail);

SepError e_out_of_memory();
SepError e_not_implemented_yet(const char *what);
SepError e_internal(const char *message);

SepError e_type_mismatch(const char *what, const char *expected_type);
SepError e_wrong_arguments(const char *message);

// ===============================================================
// Macros for easier error handling
// ===============================================================

#define or_quit() ; if (sepv_is_exception(err)) { *out_err = err; return; };
#define or_quit_with(rv) ; if (sepv_is_exception(err)) { *out_err = err; return rv; };
#define or_go(label) ; if (sepv_is_exception(err)) { *out_err = err; goto label; };
#define or_handle() ; if(sepv_is_exception(err))

#define _fail1(error) do { *out_err = error; return; } while(0)
#define _fail2(rv,error) do { *out_err = error; return rv; } while(0)
#define _fail_macro_name(_2, _1, name, ...) name
#define fail(...) _fail_macro_name(__VA_ARGS__, _fail2, _fail1)(__VA_ARGS__)

#define NO_ERROR SEPV_NO_VALUE

/*****************************************************************/

#endif
