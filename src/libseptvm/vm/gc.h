#ifndef GC_H_
#define GC_H_

/*****************************************************************
 **
 ** vm/gc.h
 **
 ** Defines the public interface of the garbage collector.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "../common/garray.h"
#include "types.h"
#include "mem.h"

// ===============================================================
//  Garbage collection
// ===============================================================

// Represents an undergoing garbage collection process.
typedef struct GarbageCollection {
	// pointer to the memory being collected
	struct ManagedMemory *memory;
	// queue of objects to mark in the mark phase (treated as a circular buffer)
	GenericArray mark_queue;
	// the index of the first active item in the mark queue
	uint32_t queue_start;
	// the numbers of total items queued for marking
	uint32_t queue_length;
} GarbageCollection;

// Performs a full collection from start to finish, both mark and sweep.
void gc_perform_full_gc();

// Marks a region of memory as being still in use.
void gc_mark_region(void *region);
// Queues an object for marking.
void gc_add_to_queue(GarbageCollection *this, SepV object);

/*****************************************************************/

#endif
