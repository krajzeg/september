#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "errors.h"

/*****************************************************************/

SepError error_create(SepErrorType type, const char *message_fmt, ...);

SepError e_file_not_found(const char *filename) { return error_create(EFileNotFound, "File '%s' does not exist.", filename); }
SepError e_not_september_file() { return error_create(ENotSeptemberFile, "This file does not seem to be a September module file."); }
SepError e_unexpected_eof() { return error_create(EUnexpectedEOF, "Encountered end of file where more data was expected."); }
SepError e_malformed_module_file(const char *detail) { return error_create(EMalformedModuleFile, "The module file seems to be incorrect: %s.", detail); }

SepError e_out_of_memory() { return error_create(EOutOfMemory, "Out of memory."); }
SepError e_not_implemented_yet(const char *what) { return error_create(ENotImplementedYet, "Missing implementation for: %s.", what); }

/*****************************************************************/

SepError error_create(SepErrorType type, const char *message_fmt, ...) {
	SepError error = {type, false, NULL, NULL};

	char *message = malloc(1024);
	va_list args;
	va_start(args, message_fmt);
	vsnprintf(message, 1024, message_fmt, args);
	error.message = message;
	va_end(args);

	return error;
}

bool error_matches(SepError error, SepErrorType type) {
	return (error.type) && (!error.handled) && (error.type & (type >> 16)) == (type & 0xFFFF);
}

void error_free(SepError error) {
	free(error.message);
	if (error.previous) {
		error_free(*(error.previous));
		free(error.previous);
	}
}

void error_report(SepError error) {
	fprintf(stderr, "%s\n", error.message);
	if (error.previous)
		error_report(*(error.previous));
}

