/*****************************************************************
 **
 ** loader.c
 **
 ** Implements the module loader API from loader.h
 **
 ***************
 ** September **
 *****************************************************************/


// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>

#include "../vm/runtime.h"

#include "../vm/vm.h"
#include "../vm/types.h"
#include "../vm/exceptions.h"

#include "decoder.h"
#include "loader.h"

// ===============================================================
//  Module loading
// ===============================================================

ModuleFinderFunc find_module;

// Must be executed by the interpreter to let the loader library know how to find
// modules.
void initialize_module_loader(ModuleFinderFunc find_module_func) {
	find_module = find_module_func;
}

// Loads the module from a given definition and returns its root object.
SepV load_module(ModuleDefinition *definition) {
	SepError err = NO_ERROR;

	// create the empty module
	SepModule *module = module_create(&rt);

	// if there is any bytecode, load it in
	if (definition->bytecode) {
		BytecodeDecoder *decoder = decoder_create(definition->bytecode);
		decoder_read_pools(decoder, module, &err);
			or_handle(EAny) { goto cleanup; }
	}

	// TODO: execute early native initializer

	if (definition->bytecode) {
		// execute the root function
		SepVM *vm = vm_create(module, rt.syntax);
		vm_initialize_root_frame(vm, module);
		SepV result = vm_run(vm);
		vm_free(vm);

		// exception?
		if (sepv_is_exception(result))
			return result;
	}

	// TODO: execute late native initializer

	// return the ready module
	return obj_to_sepv(module->root);

cleanup:
	if (module)
		module_free(module);
	return sepv_exception(exc.EInternal, sepstr_sprintf("Error reading module: %s.", err.message));
}

// Loads a module by its string name. Uses functionality delivered by the interpreter
// to find the module.
SepV load_module_by_name(SepString *module_name) {
	SepError err = NO_ERROR;
	// find the module
	ModuleDefinition *definition = find_module(module_name, &err);
		or_handle(EAny) {
			return sepv_exception(exc.EInternal,
				sepstr_sprintf("Unable to load module '%s': %s", sepstr_to_cstr(module_name), err.message));
		}

	// load from the definition
	SepV result = load_module(definition);

	// clean up
	moduledef_free(definition);
	return result;
}

// ===============================================================
//  Module definitions
// ===============================================================

// Creates a new module definition from its components.
ModuleDefinition *moduledef_create(ByteSource *bytecode, ModuleNativeCode *native) {
	ModuleDefinition *def = malloc(sizeof(ModuleDefinition));
	def->bytecode = bytecode;
	def->native = native;
	return def;
}

// Destroys a module definition and frees its components.
void moduledef_free(ModuleDefinition *this) {
	if (this->bytecode)
		this->bytecode->vt->close_and_free(this->bytecode);
	if (this->native)
		free(this->native);
	free(this);
}

// ===============================================================
//  File sources
// ===============================================================

typedef struct FileSource {
	ByteSourceVT *vt;
	FILE *file;
} FileSource;

// Gets the next byte from the file connected with the file source.
uint8_t filesource_get_byte(ByteSource *_this, SepError *out_err) {
	FileSource *this = (FileSource*)_this;
	int byte = fgetc(this->file);
	if (byte == EOF)
		fail(0, e_unexpected_eof());
	return byte;
}

// Closes the file source along with its file.
void filesource_close_and_free(ByteSource *_this) {
	FileSource *this = (FileSource*)_this;

	// close and free everything safely
	if (!this) return;
	if (this->file)
		fclose(this->file);
	free(this);
}

ByteSourceVT filesource_vt = {
	&filesource_get_byte, &filesource_close_and_free
};

// Creates a new file source pulling from a given file.
ByteSource *file_bytesource_create(const char *filename, SepError *out_err) {
	// try opening the file
	FILE *file = fopen(filename, "rb");
	if (file == NULL)
		fail(NULL, e_file_not_found(filename));

	// allocate and set up the data structure
	FileSource *source = malloc(sizeof(FileSource));
	source->vt = &filesource_vt;
	source->file = file;

	return (ByteSource*)source;
}
