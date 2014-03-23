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

// ===============================================================
//  Exit codes
// ===============================================================

enum ExitCodes {
	EXIT_OK = 0,
	EXIT_EXCEPTION_RAISED = 1,
	EXIT_NO_EXECUTION = 2
};

// ===============================================================
//  Running a module
// ===============================================================

int run_program(const char *filename) {
	SepError err = NO_ERROR;

	// create the file source
	ByteSource *bytecode = file_bytesource_create(filename, &err);
	or_handle(EAny) {
		error_report(err);
		return EXIT_NO_EXECUTION;
	};
	ModuleDefinition *definition = moduledef_create(bytecode, NULL);

	// load and run the module (run is implicit)
	SepV result = load_module(definition);

	// check for uncaught exceptions
	if (sepv_is_exception(result)) {
		// wound up with an exception, extract it
		SepV exception_object = exception_to_obj_sepv(result);

		const char *class_name, *message;
		SepV class_v = property(exception_object, "<class>");
		SepV class_name_v = property(class_v, "<name>");
		if (sepv_is_str(class_name_v))
			class_name = sepstr_to_cstr(sepv_to_str(class_name_v));
		else
			class_name = "<unknown type>";

		SepV message_sepv = property(exception_object, "message");
		if (sepv_is_str(message_sepv))
			message = sepstr_to_cstr(sepv_to_str(message_sepv));
		else
			message = "<message missing>";

		// report it
		fprintf(stderr, "Exception encountered during execution:\n");
		fprintf(stderr, "  %s: %s\n", class_name, message);

		// and clean up
		return EXIT_EXCEPTION_RAISED;
	}

	// everything OK
	return EXIT_OK;
}

int main(int argc, char **argv) {
	// == 'verify' and parse arguments
	if (argc != 2) {
		fprintf(stderr, "Usage: 09 <module file>\n");
		exit(EXIT_NO_EXECUTION);
	}
	const char *module_file_name = argv[1];

	// == initialize the runtime
	SepV globals_v = SEPV_NOTHING;
	initialize_runtime_references(globals_v);

	// == load the module
	return run_program(module_file_name);
}
