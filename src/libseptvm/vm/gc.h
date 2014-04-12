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
//  Garbage collection constants
// ===============================================================

// the "ideal" proportion of free space after a GC completes
// additional space will be allocated if there is less than GTFP%
// free space in the chunks after a GC.
#define GC_MINIMUM_FREE_PERCENTAGE 33

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

// ===============================================================
//  Registering objects
// ===============================================================

// A context is basically an array of registered objects protected from GC.
typedef struct GCContext {
	// the registered context roots
	GenericArray context_roots;
} GCContext;

// Registers an object to prevent it from being collected until the end of the current
// GC context. Every VM execution frame is an implicit GC context, but you can declare
// explicit ones with gc_start_context()/gc_end_context().
// By default, all newly allocated SepObj, SepFunc and SepString are registered to the
// current context - so you have to gc_release() them if you want them to be freed
// before the context ends.
void gc_register(SepV object);
// Releases a previously registered object. Should be used in long-lived contexts (e.g.
// C functions implementing a September loop) to release no longer needed objects for GC.
void gc_release(SepV object);
// Starts a new GC context in which you can protect objects from being collected.
void gc_start_context();
// Ends a previously started GC context.
void gc_end_context();

/*****************************************************************/

#endif
