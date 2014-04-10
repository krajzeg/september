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
#include "../common/garray.h"

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

// how many bytes are considered one unit in memory bitmaps
#define ALLOCATION_UNIT 8
// a type of size ALLOCATION_UNIT bytes (only the size matters, as it will be used for pointers)
typedef uint64_t alloc_unit_t;

/**
 * A header stored at the start of every free extent in the chunk's arena. The headers
 * make up a linked list of free extents in the chunk.
 */
typedef struct FreeBlockHeader {
	// the size of this block in allocation units
	uint32_t size;
	// the offset from the start of this free extent to the start of the next one
	// if this is the last free extent in the chunk, this will be 0
	uint32_t offset_to_next_free;
} FreeBlockHeader;

// Flags used by every used block header.
typedef struct BlockFlags {
	// was this block marked in the last GC mark phase?
	int marked : 1;
} BlockFlags;

// A header stored at the start of every used block of memory.
typedef struct UsedBlockHeader {
	// the size of this block in allocation units
	uint32_t size;
	// status
	union {
		BlockFlags flags;
		uint32_t word;
	} status;
} UsedBlockHeader;

/**
 * Represents a single chunk of managed memory.
 */
typedef struct MemoryChunk {
	// the memory backing this memory chunk
	alloc_unit_t *memory, *memory_end;
	// pointer to the first entry in the free block list
	FreeBlockHeader *free_list;
	// total allocation units in use in this chunk
	uint32_t used;
} MemoryChunk;

/**
 * Represents an indivisible chunk used for big allocations (bigger
 * than half standard chunk size).
 */
typedef struct OutsizeChunk {
	// pointer to the single block header for this entire chunk
	UsedBlockHeader *header;
	// the single block of memory represented by this chunk
	void *block;
	// the entire piece of memory backing this chunk (header + block)
	alloc_unit_t *memory;
	// the allocated size in bytes
	size_t size;
} OutsizeChunk;

/**
 * Represents the entirety of managed memory.
 */
typedef struct ManagedMemory {
	// the array of standard memory chunks currently in use
	GenericArray chunks;
	// an array of big chunks used for allocations bigger than the standard chunk size
	GenericArray outsize_chunks;
	// the size used for all the chunks
	uint32_t chunk_size;

	// statistics and limits
	uint64_t total_allocated_bytes;
	uint64_t outsize_allocated_bytes;
	uint64_t allocation_limit_before_next_gc;
} ManagedMemory;

// Initializes the memory manager. Memory will be allocated in increments of chunk_size,
// and it has to be a power of two. The minimum chunk size is 1024 bytes.
ManagedMemory *mem_initialize(uint32_t chunk_size);
// Allocates a new chunk of managed memory. Managed memory does not have
// to be freed - it will be freed automatically by the garbage collector.
void *mem_allocate(size_t bytes);

// Used for updating statistics and limits after a full GC is performed.
void mem_update_statistics();

// ===============================================================
//  Memory management information
// ===============================================================

// Returns the total number of bytes of memory that are currently in use
// (allocated and occupied by an active object).
uint64_t mem_used_bytes(ManagedMemory *this);
// Returns the total number of bytes allocated by the memory manager
// (including free space waiting for new objects and outsize allocations).
uint64_t mem_allocated_bytes(ManagedMemory *this);
// Returns the total number of bytes allocated in outsize blocks.
uint64_t mem_allocated_outsize_chunks(ManagedMemory *this);

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
	void (*free)(void *_managed_memory);
} Allocator;

// Pre-defined managed/unmanaged allocators.
extern Allocator allocator_managed;
extern Allocator allocator_unmanaged;

#endif
