/*****************************************************************
 **
 ** runtime/mem.c
 **
 ** Implementation for mem.h.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../common/debugging.h"
#include "../common/garray.h"
#include "../common/errors.h"
#include "mem.h"
#include "gc.h"
#include "types.h"
#include "vm.h"

// ===============================================================
//  Aligned memory
// ===============================================================

// Allocate new memory, with the starting address aligned at
// an even 'alignment' bytes.
void *aligned_alloc(size_t bytes, size_t alignment) {
	uint8_t *memory = (uint8_t*) malloc(bytes + alignment);
	if (memory == NULL)
		return NULL;
	
	uint8_t offset = alignment - ((intptr_t)memory) % alignment;
	memory[offset-1] = offset;
	
	return memory + offset;
}

// Reallocate a bit of aligned memory to a new size, keeping
// the alignment and contents intact. Returns a new pointer
// if the memory was moved, just like realloc().
void *aligned_realloc(void *aligned_memory, size_t new_size, size_t alignment) {
	uint8_t *memory = (uint8_t*)aligned_memory;
	uint8_t *base_memory = memory - memory[-1];
	
	base_memory = realloc(base_memory, new_size + alignment);
	if (base_memory == NULL)
		return NULL;
	
	uint8_t offset = alignment - ((intptr_t)base_memory) % alignment;
	base_memory[offset-1] = offset;
	
	return base_memory + offset;
}

// Free memory previously allocated with aligned_alloc.
void aligned_free(void *aligned_memory) {
	uint8_t *memory = (uint8_t*)aligned_memory;
	uint8_t offset = memory[-1];
	
	free(memory - offset);
}

// ===============================================================
//  Memory manager
// ===============================================================

// Called when an out of memory error occurs. The only thing we do
// is print a message describing the problem before dying.
void handle_out_of_memory() {
	fprintf(stderr, "FATAL ERROR: Out of memory. Shutting down.");
	exit(EXIT_OUT_OF_MEMORY);
}

// Allocates a new chunk of memory of a given size.
void *mem_unmanaged_allocate(size_t bytes) {
	void *memory = aligned_alloc(bytes, SEP_PTR_ALIGNMENT);
	if (!memory)
		handle_out_of_memory();
	return memory;
}

// Reallocates a chunk of memory to a new size, keeping the contents, but
// possibly moving it.
void *mem_unmanaged_realloc(void *memory, size_t new_size) {
	memory = aligned_realloc(memory, new_size, SEP_PTR_ALIGNMENT);
	if (!memory)
		handle_out_of_memory();
	return memory;
}

// Frees a chunk of unmanaged memory. Unlike managed memory, it has to be
// explicitly freed.
void mem_unmanaged_free(void *memory) {
	aligned_free(memory);
}

// ===============================================================
//  MemoryChunk internals
// ===============================================================

void *_free_block_allocate(FreeBlockHeader *block, FreeBlockHeader *previous, uint32_t units) {
	// if we would consume all but one unit, we allocate the entire block instead (there is no space for anything
	// besides the header)
	if (units == block->size - 1)
		units++;

	UsedBlockHeader *allocated;
	if (units == block->size) {
		// whole block allocated, update the linked list to remove the block entirely
		if (block->offset_to_next_free)
			previous->offset_to_next_free += block->offset_to_next_free;
		else
			previous->offset_to_next_free = 0;
		allocated = (UsedBlockHeader*)block;
	} else {
		// not the whole block, just carve out space at the end
		block->size -= units;
		allocated = (UsedBlockHeader*)((alloc_unit_t*)block + block->size);
	}

	// initialize the new block to a freshly born state
	allocated->size = units;
	allocated->status.word = 0;

	// return (one allocation unit added to skip the header)
	return ((alloc_unit_t*)allocated) + 1;
}

// Creates a fresh MemoryChunk.
MemoryChunk *_chunk_create(ManagedMemory *manager) {
	MemoryChunk *chunk = mem_unmanaged_allocate(sizeof(MemoryChunk));
	chunk->memory = mem_unmanaged_allocate(manager->chunk_size);
	chunk->memory_end = chunk->memory + (manager->chunk_size / ALLOCATION_UNIT);

	// initialize the free block list
	// first the artificial head
	FreeBlockHeader *head = chunk->free_list = (FreeBlockHeader*)chunk->memory;
	head->offset_to_next_free = 1;
	head->size = 1;

	// then the actual giant free block
	FreeBlockHeader *block = (FreeBlockHeader*)(chunk->memory + head->offset_to_next_free);
	block->size = (manager->chunk_size / ALLOCATION_UNIT) - 1;
	block->offset_to_next_free = 0; // nothing next

	return chunk;
}

// Tries to allocate memory from a given chunk. If the allocation cannot be satisfied,
// returns NULL.
void *_chunk_allocate(MemoryChunk *chunk, size_t bytes) {
	// determine actual needed allocation size in units, including block header
	size_t required_units = (bytes + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT + 1;

	// look through the free list
	FreeBlockHeader *previous = chunk->free_list;
	FreeBlockHeader *free_block = (FreeBlockHeader*)((alloc_unit_t*)previous + previous->offset_to_next_free);
	while (true) {
		if (free_block->size >= required_units) {
			// we can satisfy the allocation from this block
			return _free_block_allocate(free_block, previous, required_units);
		}

		// not enough space here, next block
		if (!free_block->offset_to_next_free) {
			// no more free blocks - no joy in this chunk
			return NULL;
		} else {
			previous = free_block;
			free_block = (FreeBlockHeader*)((alloc_unit_t*)previous + previous->offset_to_next_free);
		}
	}
}

OutsizeChunk *_outsize_chunk_create(size_t size) {
	OutsizeChunk *chunk = mem_unmanaged_allocate(sizeof(OutsizeChunk));

	size_t allocation_size = ((size / ALLOCATION_UNIT) + 2);
	chunk->memory = mem_unmanaged_allocate(allocation_size * ALLOCATION_UNIT);
	chunk->size = allocation_size * ALLOCATION_UNIT;

	// initialize the header and block
	chunk->header = (UsedBlockHeader*)chunk->memory;
	chunk->block = (void*)(chunk->memory + 1);

	chunk->header->size = allocation_size;
	chunk->header->status.word = 0;

	// return the chunk
	return chunk;
}

// ===============================================================
//  ManagedMemory internals
// ===============================================================

void _mem_add_chunks(ManagedMemory *memory, int how_many) {
	int i;
	for (i = 0; i < how_many; i++) {
		MemoryChunk *chunk = _chunk_create(memory);
		ga_push(&memory->chunks, &chunk);
	}
}

void *_mem_allocate_outsize(ManagedMemory *memory, size_t size) {
	OutsizeChunk *chunk = _outsize_chunk_create(size);
	ga_push(&memory->outsize_chunks, &chunk);
	return chunk->block;
}

void *_mem_allocate_from_any_chunk(ManagedMemory *manager, uint32_t bytes) {
	GenericArrayIterator it = ga_iterate_over(&manager->chunks);

	// go through all the chunks looking for one that can satisfy the allocation
	while (!gait_end(&it)) {
		// try this chunk
		MemoryChunk *chunk = *((MemoryChunk**)gait_current(&it));
		void *allocation = _chunk_allocate(chunk, bytes);
		if (allocation)
			return allocation;

		// did not work - maybe the next one will have enough space
		gait_advance(&it);
	}

	// no free space anywhere
	return NULL;
}

// ===============================================================
//  Managed memory public interface
// ===============================================================

// Initializes a new memory manager. Memory will be allocated in increments of chunk_size,
// and it has to be a power of two. The minimum chunk size is 1024 bytes.
ManagedMemory *mem_initialize(uint32_t chunk_size) {
	ManagedMemory *mem = mem_unmanaged_allocate(sizeof(ManagedMemory));
	mem->chunk_size = chunk_size;
	mem->total_allocated_bytes = 0;
	mem->used_bytes = 0;

	ga_init(&mem->chunks, 1, sizeof(MemoryChunk*), &allocator_unmanaged);
	ga_init(&mem->outsize_chunks, 0, sizeof(OutsizeChunk*), &allocator_unmanaged);

	// add a chunk of memory for a good start
	_mem_add_chunks(mem, 1);

	return mem;
}

// Allocates a new chunk of managed memory. Managed memory does not have
// to be freed - it will be freed automatically by the garbage collector.
void *mem_allocate(size_t bytes) {
	ManagedMemory *manager = lsvm_globals.memory;

	// handle outsize allocations (bigger than chunk_size)
	if (bytes > manager->chunk_size / 2) {
		#ifdef SEP_GC_STRESS_TEST
			gc_perform_full_gc();
		#endif
		return _mem_allocate_outsize(manager, bytes);
	}

	// defining SEP_GC_STRESS_TEST causes a full GC to happen before EVERY allocation
	// to better test the correctness of its implementation
	void *allocation;
	#ifndef SEP_GC_STRESS_TEST
		// allocate from any memory chunk
		allocation = _mem_allocate_from_any_chunk(manager, bytes);
		if (allocation)
			return allocation;
	#endif

	// no free space in any chunk - make some space by launching GC and try again
	gc_perform_full_gc();
	allocation = _mem_allocate_from_any_chunk(manager, bytes);
	if (allocation)
		return allocation;

	// still didn't work - we simply have to get a new chunk to satisfy the allocation
	log("mem", "Not enough space to allocate %d bytes, allocating new chunk.", bytes);
	_mem_add_chunks(manager, 1);
	return _mem_allocate_from_any_chunk(manager, bytes); // cannot fail with a fresh chunk available
}

// ===============================================================
//  Generalizing allocation
// ===============================================================

// Adapting unmanaged/managed allocations to the Allocator interface
void *_managed_reallocate(void *memory, size_t old_size, size_t new_size) {
	void *new_memory = mem_allocate(new_size);
	memcpy(new_memory, memory, old_size);
	return new_memory;
}
void _managed_free(void *memory) {}

void *_unmanaged_reallocate(void *memory, size_t old_size, size_t new_size) {
	return mem_unmanaged_realloc(memory, new_size);
}

// Pre-defined managed/unmanaged allocators.
Allocator allocator_managed = {&mem_allocate, &_managed_reallocate, &_managed_free};
Allocator allocator_unmanaged = {&mem_unmanaged_allocate, &_unmanaged_reallocate, &mem_unmanaged_free};
