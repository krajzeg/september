/*****************************************************************
 **
 ** main.c
 **
 ** The main entry point to the interpreter. Contains code for
 ** parsing the command line parameters, loading the main module
 ** specified, and then executing it using functionalities from
 ** libseptvm.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
// Includes
// ===============================================================

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <septvm.h>

#include "platform/platform.h"
#include "modules/modules.h"

// ===============================================================
//  Exit codes
// ===============================================================

enum ExitCodes {
	EXIT_OK = 0,
	EXIT_EXCEPTION_RAISED = 1,
	EXIT_NO_EXECUTION = 2
};

// ===============================================================
//  Exception reporting
// ===============================================================

void report_exception(SepV exception_v) {
	// wound up with an exception, extract it
	exception_v = exception_to_obj_sepv(exception_v);

	const char *class_name, *message;
	SepV class_v = property(exception_v, "<class>");
	SepV class_name_v = property(class_v, "<name>");
	if (sepv_is_str(class_name_v))
		class_name = sepv_to_str(class_name_v)->cstr;
	else
		class_name = "<unknown type>";

	SepV message_sepv = property(exception_v, "message");
	if (sepv_is_str(message_sepv))
		message = sepv_to_str(message_sepv)->cstr;
	else
		message = "<message missing>";

	// report it
	fprintf(stderr, "Exception encountered during execution:\n");
	fprintf(stderr, "  %s: %s\n", class_name, message);
}

// ===============================================================
//  Loading the runtime
// ===============================================================

SepV load_runtime() {
	return load_module_by_name(sepstr_for("runtime"));
}

// ===============================================================
//  Running a module
// ===============================================================

int run_program(const char *filename) {
	SepError err = NO_ERROR;

	// create the file source
	ByteSource *bytecode = file_bytesource_create(filename, &err);
	or_handle() {
		error_report(err);
		return EXIT_NO_EXECUTION;
	};
	ModuleDefinition *definition = moduledef_create(bytecode, NULL);
	definition->name = sepstr_for("<main>");

	// load and run the module (run is implicit)
	SepV result = load_module(definition);

	// check for uncaught exceptions
	if (sepv_is_exception(result)) {
		// wound up with an exception, extract it
		report_exception(result);
		return EXIT_EXCEPTION_RAISED;
	}

	// everything OK
	return EXIT_OK;
}

int main(int argc, char **argv) {
	// == 'verify' and parse arguments
	if (argc != 2) {
		fprintf(stderr, "Usage: 09 <module file>\n");
		return EXIT_NO_EXECUTION;
	}
	const char *module_file_name = argv[1];

	// == platform-specific initialization
	platform_initialize(argc, argv);
	libseptvm_initialize();

	// == initialize the runtime
	gc_start_context();
		initialize_module_loader(find_module_files);
		SepV globals_v = load_runtime();
		if (sepv_is_exception(globals_v)) {
			report_exception(globals_v);
			return EXIT_NO_EXECUTION;
		}
		initialize_runtime_references(globals_v);
	gc_end_context();

	// == load the module
	return run_program(module_file_name);
}
