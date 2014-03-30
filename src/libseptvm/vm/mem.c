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

#include "mem.h"
#include "../common/errors.h"

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
void *mem_unmanaged_allocate(uint32_t bytes) {
	void *memory = aligned_alloc(bytes, 8);
	if (!memory)
		handle_out_of_memory();
	return memory;
}

// Reallocates a chunk of memory to a new size, keeping the contents, but
// possibly moving it.
void *mem_unmanaged_realloc(void *memory, uint32_t new_size) {
	memory = aligned_realloc(memory, new_size, 8);
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
//  Memory manager
// ===============================================================

// Allocates a new chunk of managed memory. Managed memory does not have
// to be freed - it will be freed automatically by the garbage collector.
void *mem_allocate(uint32_t bytes) {
	// TODO: mocked for now
	return mem_unmanaged_allocate(bytes);
}
