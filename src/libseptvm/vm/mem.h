#ifndef _SEP_MEM_H
#define _SEP_MEM_H

/*****************************************************************
 **
 ** runtime/mem.h
 **
 ** Contains low-level functions for handling memory.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stddef.h>
#include <stdint.h>
#include "../common/errors.h"

// ===============================================================
//  Constants
// ===============================================================

// The alignment required for all SepObjs and other SepVs.
// This is needed because the SepV representation of pointers
// truncate the lower 3-bits, and we have to be sure we can
// point to the right place.
#define SEP_PTR_ALIGNMENT 8

// The exit code used when we bail due to an out of memory error.
#define EXIT_OUT_OF_MEMORY 16

// ===============================================================
//  Unmanaged memory
// ===============================================================

/**
 * Unmanaged memory is a thin wrapper on malloc, ensuring 8-byte alignment
 * and some very basic error handling.
 */

// Allocates a new chunk of memory of a given size.
void *mem_unmanaged_allocate(size_t bytes);
// Reallocates a chunk of memory to a new size, keeping the contents, but
// possibly moving it.
void *mem_unmanaged_realloc(void *memory, size_t new_size);
// Frees a chunk of unmanaged memory. Unlike managed memory, it has to be
// explicitly freed.
void mem_unmanaged_free(void *memory);

// ===============================================================
//  Managed memory
// ===============================================================

// Allocates a new chunk of managed memory. Managed memory does not have
// to be freed - it will be freed automatically by the garbage collector.
void *mem_allocate(size_t bytes);

// ===============================================================
//  Generalizing allocation
// ===============================================================

/**
 * Some objects want to be able to work with both managed and unmanaged memory,
 * like the GenericArray. The Allocator interface is a way to do that.
 */

typedef struct Allocator {
	void *(*allocate)(size_t bytes);
	void *(*reallocate)(void *original, size_t old_size, size_t new_size);
	void (*free)(void *memory);
} Allocator;

// Pre-defined managed/unmanaged allocators.
extern Allocator allocator_managed;
extern Allocator allocator_unmanaged;

#endif
