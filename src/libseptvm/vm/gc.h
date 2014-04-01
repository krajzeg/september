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
	// pointer to the VM executing the code
	struct SepVM *vm;
	// pointer to the memory being collected
	struct ManagedMemory *memory;
	// queue of objects to mark in the mark phase (treated as a circular buffer)
	GenericArray mark_queue;
	// the indices of the beginning and end of queued items in the mark_queue buffer
	// the 'start' index is the first active item
	// the 'end' index is the first free item
	int queue_start, queue_end;
} GarbageCollection;

// Performs a full collection from start to finish, both mark and sweep.
void gc_perform_full_gc(struct SepVM *vm);
// Queues an object for marking.
void gc_add_to_queue(GarbageCollection *this, SepV object);

/*****************************************************************/

#endif
