/*****************************************************************
 **
 ** vm/gc.c
 **
 ** Implementation for the garbage collector.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <assert.h>
#include <stdio.h>

#include "../common/debugging.h"
#include "gc.h"
#include "mem.h"
#include "types.h"
#include "objects.h"
#include "functions.h"
#include "arrays.h"
#include "vm.h"

// ===============================================================
//  Helpers
// ===============================================================

#define used_block_header(block) ((UsedBlockHeader*)((char*)(block) - ALLOCATION_UNIT))

// ===============================================================
//  Registering objects
// ===============================================================

// Registers an object to prevent it from being collected until the end of the current
// GC context. Every VM execution frame is an implicit GC context, but you can declare
// explicit ones with gc_start_context()/gc_end_context().
// By default, all newly allocated SepObj, SepFunc and SepString are registered to the
// current context - so you have to gc_release() them if you want them to be freed
// before the context ends.
void gc_register(SepV object) {
	// try an execution-frame-based context
	ExecutionFrame *frame = vm_current_frame();
	if (frame)
		frame_register(frame, object);
}

// Releases a previously registered object. Should be used in long-lived contexts (e.g.
// C functions implementing a September loop) to release no longer needed objects for GC.
void gc_release(SepV object) {
	// try an execution-frame-based context
	ExecutionFrame *frame = vm_current_frame();
	if (frame)
		frame_release(frame, object);
}

// Starts a new GC context in which you can protect objects from being collected.
void gc_start_context() {}

// Ends a previously started GC context.
void gc_end_context() {}

// ===============================================================
//  Mark queue
// ===============================================================

SepV gc_next_in_queue(GarbageCollection *this) {
	// are we done yet?
	if (this->queue_length == 0)
		return SEPV_NO_VALUE;

	// no - get first object in queue
	SepV object = *((SepV*)ga_get(&this->mark_queue, this->queue_start));
	this->queue_start = (this->queue_start + 1) % ga_length(&this->mark_queue);
	this->queue_length--;

	return object;
}


void gc_add_to_queue(GarbageCollection *this, SepV object) {
	// non-pointer types do not use managed memory
	if (!sepv_is_pointer(object))
		return;

	// already marked?
	void *ptr = sepv_to_pointer(object);
	if (used_block_header(ptr)->status.flags.marked)
		return;

	// add to the queue buffer
	uint32_t buffer_length = ga_length(&this->mark_queue);
	bool buffer_full = this->queue_length == buffer_length;
	if (!buffer_full) {
		// buffer not full yet, add at end
		uint32_t index = (this->queue_start + this->queue_length) % buffer_length;
		ga_set(&this->mark_queue, index, &object);
		this->queue_length++;
	} else {
		// buffer full - expand the buffer
		// this does not always insert at the end of the queue, but the order
		// doesn't really matter
		ga_push(&this->mark_queue, &object);
		this->queue_length++;
	}
}

// ===============================================================
//  Mark phase
// ===============================================================

// Marks a region of memory as being still in use.
void gc_mark_region(void *region) {
	if (!region)
		return;

	// checks if the address is correct by looking at the "supposed" header
	assert(*(((uint32_t*)region) - 1) <= 1);

	used_block_header(region)->status.flags.marked = 1;
}

// Queues objects reachable from a SepObj for marking and marks its internal
// memory regions.
void gc_mark_and_queue_obj(GarbageCollection *this, SepObj *object) {
	// mark the property map region
	gc_mark_region(object->props.entries);

	// queue property values
	if (object->props.entries) {
		PropertyIterator it = props_iterate_over(object);
		while (!propit_end(&it)) {
			// the name and the value of this property are not to be collected
			gc_add_to_queue(this, str_to_sepv(propit_name(&it)));

			// the value is retrieved directly from the slot without using
			// slot->vt->retrieve to avoid allocations in GC
			gc_add_to_queue(this, propit_slot(&it)->value);

			// advance the iterator
			propit_next(&it);
		}
	}

	// the prototypes are not to be collected
	gc_add_to_queue(this, object->prototypes);

	// arrays need to collect their elements too
	if (object->traits.representation == REPRESENTATION_ARRAY) {
		SepArray *array = (SepArray*)object;
		if (array->array.start) {
			// mark the array's storage area as used
			gc_mark_region(array->array.start);
			// queue all elements of this array
			SepArrayIterator ait = array_iterate_over(array);
			while (!arrayit_end(&ait)) {
				gc_add_to_queue(this, arrayit_next(&ait));
			}
		}
	}
}

// Queues objects reachable from a SepFunc for marking and marks its internal
// memory regions.
void gc_mark_and_queue_func(GarbageCollection *this, SepFunc *func) {
	// delegate - each function type has different logic here
	func->vt->mark_and_queue(func, this);
}

// Marks any value passed in as a SepV and queues all other objects
// reachable from it for marking.
void gc_mark_one_object(GarbageCollection *this, SepV object) {
	// if it's not a pointer type, we have nothing to do, as no memory was allocated for it
	if (!sepv_is_pointer(object))
		return;

	// mark the region itself as used
	void *ptr = sepv_to_pointer(object);
	gc_mark_region(ptr);

	// in turn, queue anything reference from this object for marking
	// and do any additional work specific types need
	switch (sepv_type(object)) {
		case SEPV_TYPE_OBJECT:
		case SEPV_TYPE_EXCEPTION:
			gc_mark_and_queue_obj(this, (SepObj*)ptr);
			break;
		case SEPV_TYPE_FUNC:
			gc_mark_and_queue_func(this, (SepFunc*)ptr);
			break;
	}
}

// Performs the entire mark phase in one shot
void gc_mark_all(GarbageCollection *this) {
	// collect GC roots from the VM
	vm_queue_gc_roots(this);

	int root_count = this->queue_length;
	log("mem", "Starting GC mark phase with %d roots.", root_count);

	// mark all objects, collecting references from them
	SepV object = gc_next_in_queue(this);
	while (object != SEPV_NO_VALUE) {
		gc_mark_one_object(this, object);
		object = gc_next_in_queue(this);
	}
}

// ===============================================================
//  Sweep phase
// ===============================================================

typedef enum MemoryBlockType {
	BLK_FREE, BLK_GARBAGE, BLK_IN_USE
} MemoryBlockType;

void gc_sweep_chunk(GarbageCollection *this, MemoryChunk *chunk) {
	alloc_unit_t *memory = chunk->memory;
	alloc_unit_t *memory_end = chunk->memory_end;
	alloc_unit_t *next_free = memory + chunk->free_list->offset_to_next_free;
	alloc_unit_t *current_block = memory + 1;

	FreeBlockHeader *last_free_block = chunk->free_list;
	MemoryBlockType last_seen = BLK_IN_USE, current_block_type;
	while (current_block < memory_end) {
		// recognize what type of block we are in
		if (next_free == current_block) {
			current_block_type = BLK_FREE;
		} else {
			current_block_type = ((UsedBlockHeader*)current_block)->status.flags.marked ? BLK_IN_USE : BLK_GARBAGE;
		}

		// act based on the block type
		if (current_block_type == BLK_FREE || current_block_type == BLK_GARBAGE) {
			// these blocks are handled similarly, but have different headers
			uint32_t current_block_size;
			switch(current_block_type) {
				case BLK_FREE: {
					// update our internal 'next free' pointer
					FreeBlockHeader *free_header = (FreeBlockHeader*)current_block;
					next_free = current_block + free_header->offset_to_next_free;
					// extract the size
					current_block_size = free_header->size;
					break;
				}

				case BLK_GARBAGE: {
					UsedBlockHeader *used_header = (UsedBlockHeader*)current_block;
					current_block_size = used_header->size;
					break;
				}

				default: {}
			}

			// fill freed memory with an identifiable pattern
			debug_only(
				int i;
				for (i = 0; i < current_block_size; i++)
					current_block[i] = 0xEFBEEFBEEFBEEFBEull;
			);

			// mark this block as free
			if (last_seen == BLK_FREE) {
				// previous block was free - just enlarge it with the space from this block
				last_free_block->size += current_block_size;
			} else {
				// previous block was not free - we'll become a new free block
				// update the linked list in the previous free block
				last_free_block->offset_to_next_free = current_block - ((alloc_unit_t*)last_free_block);
				// update internal state
				last_seen = BLK_FREE;
				last_free_block = (FreeBlockHeader*)current_block;
				last_free_block->size = current_block_size;
			}

			// move to next block
			current_block += current_block_size;

		} else {
			// this block is still in use - unmark it and leave it alone
			UsedBlockHeader *used_header = (UsedBlockHeader*)current_block;
			// remove the mark
			used_header->status.flags.marked = 0;
			// update internal state
			last_seen = BLK_IN_USE;
			// move to next
			current_block += used_header->size;
		}
	}

	// fix up the last free block - mark it as the tail in the free block linked list
	last_free_block->offset_to_next_free = 0;
}


void gc_sweep_all(GarbageCollection *this) {
	log0("mem", "GC mark phase complete, starting the sweep phase.");

	GenericArray *chunks = &this->memory->chunks;
	GenericArrayIterator it = ga_iterate_over(chunks);
	while (!gait_end(&it)) {
		MemoryChunk *chunk = *((MemoryChunk**)gait_current(&it));
		gc_sweep_chunk(this, chunk);
		gait_advance(&it);
	}
}

// ===============================================================
//  Public interface
// ===============================================================

// Creates a new garbage collection process for a given VM.
GarbageCollection *gc_create() {
	GarbageCollection *gc = mem_unmanaged_allocate(sizeof(GarbageCollection));

	gc->memory = lsvm_globals.memory;
	ga_init(&gc->mark_queue, 32, sizeof(SepV), &allocator_unmanaged);
	gc->queue_start = 0;
	gc->queue_length = 0;

	return gc;
}

// Frees the GC object.
void gc_free(GarbageCollection *this) {
	ga_free_entries(&this->mark_queue);
	mem_unmanaged_free(this);
}

// Performs a full collection from start to finish, both mark and sweep.
void gc_perform_full_gc() {
	log0("mem", "Starting a full garbage collection.");

	GarbageCollection *collection = gc_create();
	gc_mark_all(collection);
	gc_sweep_all(collection);
	gc_free(collection);

	log0("mem", "Full garbage collection completed.");
}

