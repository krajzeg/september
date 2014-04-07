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
#include "../vm/gc.h"
#include "../vm/types.h"
#include "../vm/exceptions.h"

#include "decoder.h"
#include "loader.h"

// ===============================================================
//  Module loading
// ===============================================================

ModuleFinderFunc _find_module;

// Must be executed by the interpreter to let the loader library know how to find
// modules.
void initialize_module_loader(ModuleFinderFunc find_module_func) {
	_find_module = find_module_func;
}

// Loads the module from a given definition and returns its root object.
SepV load_module(ModuleDefinition *definition) {
	SepError err = NO_ERROR;

	// start a GC context to make sure objects newly allocated by our module
	// don't disappear from under it
	gc_start_context();

	// create the empty module
	SepModule *module = module_create(&rt);

	ModuleNativeCode *native = definition->native;
	ByteSource *bytecode = definition->bytecode;

	// if there is any bytecode, load it in
	if (bytecode) {
		BytecodeDecoder *decoder = decoder_create(bytecode);
		decoder_read_pools(decoder, module, &err);
			or_handle(EAny) { goto error_handler; }
	}

	// execute early initialization, if any
	if (native) {
		if (!native->initialize_slave_vm)
			return sepv_exception(exc.EInternal, sepstr_for("Invalid September shared object: no initialize_slave_vm function."));
		native->initialize_slave_vm(&lsvm_globals, &err)
			or_handle(EAny) { goto error_handler; }
	}

	if (native && native->early_initializer) {
		ModuleInitFunc early_initialization = native->early_initializer;
		early_initialization(module, &err);
			or_handle(EAny) { goto error_handler; }
	}

	// execute bytecode, if any
	if (bytecode) {
		// execute the root function in the currently running VM
		SepVM *vm = vm_current();
		SepV result;
		if (!vm) {
			// no current VM, create a new one just for the module
			vm = vm_create(module, rt.syntax);
			vm_initialize_root_frame(vm, module);
			result = vm_run(vm);
			vm_free(vm);
		} else {
			// use current VM
			CodeBlock *root_block = bpool_block(module->blocks, 1);
			InterpretedFunc *root_func = ifunc_create(root_block, obj_to_sepv(module->root));
			result = vm_invoke_in_scope(vm, func_to_sepv(root_func), obj_to_sepv(module->root), 0).value;
		}

		// exception?
		if (sepv_is_exception(result))
			return result;
	}

	// execute early initialization, if any
	if (native && native->late_initializer) {
		ModuleInitFunc late_initialization = native->late_initializer;
		late_initialization(module, &err);
			or_handle(EAny) { goto error_handler; }
	}

	// end the GC context - any objects to be kept must be referenced by the module root after this point
	gc_end_context();

	// return the ready module
	return obj_to_sepv(module->root);

error_handler:
	if (module) module_free(module);
	gc_end_context();

	return sepv_exception(exc.EInternal, sepstr_sprintf("Error reading module: %s.", err.message));
}

// Loads a module by its string name. Uses functionality delivered by the interpreter
// to find the module.
SepV load_module_by_name(SepString *module_name) {
	// start a context for the module to ensure we have some control over GC even without a VM
	gc_start_context();

	SepError err = NO_ERROR;
	SepV result = SEPV_NOTHING;

	// find the module
	ModuleDefinition *definition = NULL;
	definition = _find_module(module_name, &err);
		or_handle(EAny) {
			result = sepv_exception(exc.EInternal,
				sepstr_sprintf("Unable to load module '%s': %s", module_name->cstr, err.message));
			goto cleanup;
		}

	// load from the definition
	result = load_module(definition);

cleanup:
	if (definition) moduledef_free(definition);
	gc_end_context();

	return result;
}

// ===============================================================
//  Module definitions
// ===============================================================

// Creates a new module definition from its components.
ModuleDefinition *moduledef_create(ByteSource *bytecode, ModuleNativeCode *native) {
	ModuleDefinition *def = mem_unmanaged_allocate(sizeof(ModuleDefinition));
	def->bytecode = bytecode;
	def->native = native;
	return def;
}

// Destroys a module definition and frees its components.
void moduledef_free(ModuleDefinition *this) {
	if (this->bytecode)
		this->bytecode->vt->close_and_free(this->bytecode);
	if (this->native)
		mem_unmanaged_free(this->native);
	mem_unmanaged_free(this);
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
	mem_unmanaged_free(this);
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
	FileSource *source = mem_unmanaged_allocate(sizeof(FileSource));
	source->vt = &filesource_vt;
	source->file = file;

	return (ByteSource*)source;
}
