/*****************************************************************
 **
 ** september.c
 **
 ** The main entry point to the interpreter. Contains code for
 ** parsing the command line parameters, loading the main module
 ** specified, and then executing it using functionalities from
 ** other files.
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

#include "common/errors.h"
#include "common/debugging.h"
#include "vm/module.h"
#include "vm/objects.h"
#include "vm/arrays.h"
#include "vm/types.h"
#include "vm/exceptions.h"
#include "vm/vm.h"
#include "io/decoder.h"
#include "runtime/runtime.h"

// ===============================================================
//  Exit codes
// ===============================================================

enum ExitCodes {
	EXIT_OK = 0,
	EXIT_EXECUTION_FAILED = 1,
	EXIT_EXCEPTION_RAISED = 2,
	EXIT_NO_EXECUTION = 3
};

// ===============================================================
//  Reading a module
// ===============================================================

SepModule *read_module_from_file(const char *filename, SepError *out_err) {
	SepError err = NO_ERROR;

	// open file
	FileSource *source = filesource_open(filename, &err);
	or_quit_with(NULL);

	// decode the contents
	Decoder *decoder = decoder_create((DecoderSource*) source);
	SepModule *module = module_create(obj_Globals, obj_Syntax);
	decoder_read_module(decoder, module, &err);
		or_go(cleanup_after_error);

	// cleanup and return
	decoder_free(decoder);
	return module;

cleanup_after_error:
	// cleanup and fail
	module_free(module);
	decoder_free(decoder);
	fail(NULL, err);
}

// ===============================================================
//  Main function
// ===============================================================

int run_program(SepModule *module) {
	// create the VM
	SepVM *vm = vm_create(module, obj_Syntax);

	// run the code
	SepV result = vm_run(vm);

	if (sepv_is_exception(result)) {
		// wound up with an exception, extract it
		SepV exception_object = exception_to_obj_sepv(result);
		SepV message_sepv =
				sepv_get(exception_object, sepstr_create("message")).value;
		const char *message = sepstr_to_cstr(sepv_to_str(message_sepv));

		// report it
		fprintf(stderr, "Exception encountered during execution:\n");
		fprintf(stderr, "\t%s\n", message);

		// and clean up
		vm_free(vm);
		return EXIT_EXCEPTION_RAISED;
	}

	// everything OK
	vm_free(vm);
	return EXIT_OK;
}

int main(int argc, char **argv) {
	SepError err = NO_ERROR;

	// == 'verify' and parse arguments
	if (argc != 2) {
		fprintf(stderr, "Usage: september <module file>\n");
		exit(EXIT_NO_EXECUTION);
	}
	const char *module_file_name = argv[1];

	// == initialize the runtime
	initialize_runtime();

	// == load the module
	SepModule *module = read_module_from_file(module_file_name, &err);
	or_handle(EAny) {
		goto handle_errors;
	}

	// == run tests
	int exit_code = run_program(module);

	// == clean up and complete execution
	module_free(module);
	return exit_code;

	handle_errors: error_report(err);
	return EXIT_NO_EXECUTION;
}
