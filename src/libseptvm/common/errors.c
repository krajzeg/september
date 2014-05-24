#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../vm/mem.h"
#include "../vm/support.h"
#include "../vm/runtime.h"
#include "errors.h"

/*****************************************************************/

SepError e_file_not_found(const char *filename) { return exception(exc.EInternal, "File '%s' does not exist.", filename); }
SepError e_not_september_file() { return exception(exc.EInternal, "This file does not seem to be a September module file."); }
SepError e_unexpected_eof() { return exception(exc.EInternal, "Encountered end of file where more data was expected."); }
SepError e_malformed_module_file(const char *detail) { return exception(exc.EInternal, detail); }
SepError e_module_not_found(const char *module_name) { return exception(exc.EInternal, module_name); }

SepError e_out_of_memory() { return exception(exc.EInternal, "Out of memory."); }
SepError e_not_implemented_yet(const char *what) { return exception(exc.EInternal, what); }
SepError e_internal(const char *message) { return exception(exc.EInternal, message); }
SepError e_type_mismatch(const char *what, const char *expected_type) { return exception(exc.EWrongType, expected_type); }
SepError e_wrong_arguments(const char *message) { return exception(exc.EWrongArguments, message); }

/*****************************************************************/

void error_report(SepError error) {
	SepError err = NO_ERROR;
	fprintf(stderr, "%s\n", prop_as_str(error, "message", &err)->cstr);
}
