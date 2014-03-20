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

// ===============================================================
//  Working with aligned memory
// ===============================================================

/**
 * The functions here are provided because the availability of
 * various 'standard' aligned malloc functions varies heavily
 * between compilers.
 */

// Allocate new memory, with the starting address aligned at
// an even 'alignment' bytes.
void *aligned_alloc(size_t bytes, size_t alignment);
// Reallocate a bit of aligned memory to a new size, keeping
// the alignment intact.
void *aligned_realloc(void *aligned_memory, size_t new_size, size_t alignment);
// Free memory previously allocated with aligned_alloc.
void aligned_free(void *aligned_memory);

/*****************************************************************/

void *mem_allocate(uint32_t bytes);
void mem_free(void *memory);

#endif
