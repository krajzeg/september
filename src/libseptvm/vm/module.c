/*****************************************************************
 **
 ** runtime/module.c
 **
 ** Implementation for module.h.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#include "../common/debugging.h"

#include "../vm/runtime.h"
#include "../vm/support.h"
#include "mem.h"
#include "objects.h"
#include "functions.h"
#include "exceptions.h"
#include "arrays.h"
#include "module.h"

// ===============================================================
//  Modules - public
// ===============================================================

// Creates a new module.
SepModule *module_create(char *name){
	// allocate and initialize
	SepModule *module = mem_unmanaged_allocate(sizeof(SepModule));
	module->runtime = &rt;
	module->name = name;
	module->blocks = NULL;
	module->constants = NULL;

	// set-up root object
	SepObj *root_obj = obj_create();

	SepArray *prototypes = array_create(2);
	array_push(prototypes, obj_to_sepv(rt.syntax));
	array_push(prototypes, obj_to_sepv(rt.globals));
	root_obj->prototypes = obj_to_sepv(prototypes);

	obj_add_field(root_obj, "module", obj_to_sepv(root_obj));

	module->root = root_obj;

	// return
	return module;
}

// Deinitializes and frees the memory taken by this module.
void module_free(SepModule *this) {
	if (!this) return;

	bpool_free(this->blocks);
	cpool_free(this->constants);
	mem_unmanaged_free(this);
}

// ===============================================================
//  Constant pools - internal
// ===============================================================

// Resizes the data block of the pool to a given number of bytes.
// Used internally to accomodate new constants being added.
void _cpool_resize(ConstantPool *this, uint32_t new_data_size) {
	log("cpool", "Resizing to %d bytes.", new_data_size);

	uint8_t *old_data = this->data;

	// reallocate data, move pointers to their proper spaces
	this->data = mem_unmanaged_realloc(this->data, new_data_size);
	ptrdiff_t offset = this->data - old_data;
	this->data_alloc_ptr += offset;
	this->data_end_ptr = this->data + new_data_size;

	// CAUTION: black magic here
	// fix SepVs in this pool to point to the right addresses again
	SepV *sepvs = (SepV*)this->data;
	uint32_t index;
	for (index = 0; index < this->constant_count; index++) {
		if (sepv_is_str(sepvs[index])) {
			// move the shifted pointer inside the SepV by 'offset' bytes
			sepvs[index] += (offset >> 3);
		}
	}

}

// Allocates a number of bytes from the internal data block of
// a constant pool. The data block will be resized if there is
// insufficient space. All storage for constants is acquired
// in this way.
void *_cpool_alloc(ConstantPool *this, size_t bytes) {
	// round the allocation size to maintain alignment
	size_t actual_size = (bytes + SEP_PTR_ALIGNMENT - 1) / SEP_PTR_ALIGNMENT * SEP_PTR_ALIGNMENT;

	// check if we have enough memory allocated
	if (this->data_alloc_ptr + actual_size > this->data_end_ptr) {
		// we have to enlarge the pool's memory first
		size_t sepvs_size = sizeof(SepV) * this->max_constants;
		size_t current_data_size = this->data_end_ptr - this->data - sepvs_size;
		_cpool_resize(this, sepvs_size + current_data_size * 2 + actual_size);
	}

	// now that we have the memory...
	void *memory = this->data_alloc_ptr;
	this->data_alloc_ptr += actual_size;
	return memory;
}

// ===============================================================
//  Constant pools - public
// ===============================================================

// Creates a new constant pool capable of housing 'num_constants'
// values.
ConstantPool *cpool_create(uint32_t num_constants) {
	// allocate pool and set up properties
	ConstantPool *pool = mem_unmanaged_allocate(sizeof(ConstantPool));
	pool->constant_count = 0;
	pool->max_constants = num_constants;

	// calculate initial size - will grow almost definitely
	size_t bytes_for_sepvs = sizeof(SepV) * num_constants;
	size_t initial_memory_size = bytes_for_sepvs + 4;

	// allocate space for constants and set initial pointers
	pool->data = mem_unmanaged_allocate(initial_memory_size);
	pool->data_alloc_ptr = pool->data + bytes_for_sepvs;
	pool->data_end_ptr = pool->data + initial_memory_size;

	// return the pool
	return pool;
}

// Frees the constant pool and all its contents.
void cpool_free(ConstantPool *this) {
	if (!this) return;
	mem_unmanaged_free(this->data);
	mem_unmanaged_free(this);
}

// Adds a new string constant at the next index.
SepV cpool_add_string(ConstantPool *this, const char *c_string) {
	log("cpool", "Adding constant %d: '%s'.", this->constant_count, c_string);

	size_t size = sepstr_allocation_size(c_string);

	// we allocate 8 extra bytes for padding before the string itself
	// this padding is required by the garbage collector and simulates
	// a "used block" header the memory managed would use if this string
	// was dynamically allocated
	void *memory = _cpool_alloc(this, size + 8);
	UsedBlockHeader *padding_block = memory;
	padding_block->status.word = 0;
	padding_block->status.flags.marked = 1;
	padding_block->size = 0xCAFEBABE;

	// time for the string itself
	SepString *string = (SepString*)(memory + 8);
	sepstr_init(string, c_string);

	SepV *sepvs = (SepV*)this->data;
	return sepvs[this->constant_count++] = str_to_sepv(string);
}

// Adds a new integer constant at the next index.
SepV cpool_add_int(ConstantPool *this, SepInt integer) {
	log("cpool", "Adding constant %d: %lld", this->constant_count, integer);

	SepV *sepvs = (SepV*)this->data;
	return sepvs[this->constant_count++] = int_to_sepv(integer);
}

// Fetches a constant under a given 'index' from the pool.
// Indices start from 1, since this is the format used in bytecode.
SepV cpool_constant(ConstantPool *this, uint32_t index) {
	if ((index < 1) || (index > this->max_constants))
		return sepv_exception(exc.EInternal, sepstr_sprintf("Constant index %d out of bounds.", index));
	return ((SepV*)this->data)[index-1];
}

// ===============================================================
//  Block pools - private
// ===============================================================

#define BPOOL_GROWTH_FACTOR 1.5f

#define rebase(original_ptr) if (original_ptr) { (original_ptr) = (void*)(this->memory + ((uint8_t*)(original_ptr) - original_base)); }

// Ensures that at least 'bytes' more bytes will fit in the pool,
// resizing the underlying array if necessary.
void _bpool_ensure_fit(BlockPool *this, uint32_t bytes) {
	// check free space
	if (this->memory_end - this->position < bytes) {
		// not enough, reallocate with a bigger block
		ptrdiff_t current_size = (this->memory_end - this->memory);
		uint32_t new_size = (uint32_t)(current_size * BPOOL_GROWTH_FACTOR + bytes);

		// store base so we can fix pointers after moving
		uint8_t *original_base = this->memory;

		// move the memory block
		this->memory = mem_unmanaged_realloc(this->memory, new_size);
		this->memory_end = this->memory + new_size;

		// fix up pointers (block index is not fixed up as its always the last allocation)
		rebase(this->position);
		rebase(this->current_block);

		// fix up internal pointers in blocks
		CodeBlock *block = (CodeBlock*)this->memory;
		int i;
		for (i = 0; i < this->total_blocks; i++) {
			// fix this block
			rebase(block->parameters);
			rebase(block->instructions);
			rebase(block->instructions_end);

			// go to next block
			block = (CodeBlock*)block->instructions_end;
		}
	}
}

#undef rebase

// ===============================================================
//  Block pools - public
// ===============================================================

// Creates a new, empty block pool.
BlockPool *bpool_create(SepModule *module, uint32_t initial_memory_size) {
	BlockPool *pool = mem_unmanaged_allocate(sizeof(BlockPool));

	// intitialize fields
	pool->module = module;
	pool->memory = mem_unmanaged_allocate(initial_memory_size);
	pool->memory_end = pool->memory + initial_memory_size;
	pool->block_index = NULL;
	pool->current_block = NULL;
	pool->position = pool->memory;
	pool->total_blocks = 0;

	// return
	return pool;
}

// Starts a new code block.
CodeBlock *bpool_start_block(BlockPool *this, int parameter_count) {
	assert(!this->current_block);

	// ensure we have enough memory
	size_t parameters_size = sizeof(FuncParam) * parameter_count;
	size_t required_memory = sizeof(CodeBlock) + parameters_size;
	_bpool_ensure_fit(this, required_memory);

	// allocate block
	CodeBlock *block = (CodeBlock*)(this->position);
	this->position += required_memory;

	// set the block up
	block->module = this->module;
	block->parameter_count = parameter_count;
	block->instructions = (CodeUnit*)(this->position);
	block->parameters = (FuncParam*)(this->position - parameters_size);

	// remember it
	this->current_block = block;
	this->total_blocks++;

	// return block
	return block;
}

// Writes a single operation or its argument to the current block's
// instruction stream.
void bpool_write_code(BlockPool *this, CodeUnit code) {
	// make sure we have enough space...
	_bpool_ensure_fit(this, sizeof(CodeUnit));
	// ...write at current position...
	(*(CodeUnit*)(this->position)) = code;
	// ... and move to the next
	this->position += sizeof(CodeUnit);
}

// Terminates the current block and returns its index.
uint32_t bpool_end_block(BlockPool *this) {
	assert(this->current_block);

	// mark the end of instructions
	this->current_block->instructions_end = (CodeUnit*)this->position;
	// close the block
	this->current_block = NULL;

	// return the index
	return (this->total_blocks - 1);
}

// Seals the pool - no more blocks can be added after this point.
// The block pool is not usable before it's sealed.
void bpool_seal(BlockPool *this) {
	// allocate the index
	_bpool_ensure_fit(this, sizeof(CodeBlock*) * this->total_blocks);
	this->block_index = (CodeBlock**)(this->position);

	// generate the index
	int i;
	CodeBlock *block = (CodeBlock*)this->memory;
	for (i = 0; i < this->total_blocks; i++) {
		// store in index
		this->block_index[i] = block;
		// move to next block
		block = (CodeBlock*)block->instructions_end;
	}

	// seal the pool, preventing more writing
	this->position = NULL;
}

// Retrieves a CodeBlock from the pool.
// Indices start from 1 since this is the format used in bytecode.
CodeBlock *bpool_block(BlockPool *this, uint32_t index) {
	if ((index < 1) || (index > this->total_blocks))
		return NULL;
	return this->block_index[index-1];
}

// Frees a block pool.
void bpool_free(BlockPool *this) {
	if (!this) return;

	mem_unmanaged_free(this->memory);
	mem_unmanaged_free(this);
}

// ===============================================================
//  Private objects
// ===============================================================

// Registers an object as a private value in a module, making sure it never gets GC'd.
void module_register_private(SepModule *module, SepV value) {
	// create or find the privates array
	Slot *slot = props_find_prop(module->root, sepstr_for("<private>"));
	SepArray *privates;
	if (!slot) {
		privates = array_create(1);
		obj_add_field(module->root, "<private>", obj_to_sepv(privates));
	} else {
		privates = sepv_to_array(slot->value);
	}

	// store the value
	array_push(privates, value);
}
