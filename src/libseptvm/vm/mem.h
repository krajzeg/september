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
void *mem_unmanaged_allocate(uint32_t bytes);
// Reallocates a chunk of memory to a new size, keeping the contents, but
// possibly moving it.
void *mem_unmanaged_realloc(void *memory, uint32_t new_size);
// Frees a chunk of unmanaged memory. Unlike managed memory, it has to be
// explicitly freed.
void mem_unmanaged_free(void *memory);

// ===============================================================
//  Managed memory
// ===============================================================

// Allocates a new chunk of managed memory. Managed memory does not have
// to be freed - it will be freed automatically by the garbage collector.
void *mem_allocate(uint32_t bytes);


#endif
