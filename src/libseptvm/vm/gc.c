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
#include <string.h>

#include "../common/debugging.h"
#include "../libmain.h"
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
	if (frame) {
		frame_register(frame, object);
		return;
	}

	// do we have an active GC context?
	uint32_t length;
	if ((length = ga_length(lsvm_globals.gc_contexts))) {
		GCContext *context = *((GCContext**)ga_get(lsvm_globals.gc_contexts, length-1));
		ga_push(&context->context_roots, &object);
		return;
	}

	// no place to pin our object to - should never happen
	assert(false);
}

// Releases a previously registered object. Should be used in long-lived contexts (e.g.
// C functions implementing a September loop) to release no longer needed objects for GC.
void gc_release(SepV object) {
	// try an execution-frame-based context
	ExecutionFrame *frame = vm_current_frame();
	if (frame) {
		frame_release(frame, object);
		return;
	}

	// do we have an active GC context?
	uint32_t length;
	if ((length = ga_length(lsvm_globals.gc_contexts))) {
		GCContext *context = *((GCContext**)ga_get(lsvm_globals.gc_contexts, length-1));
		ga_remove(&context->context_roots, &object);
		return;
	}

	// no place to pin our object to - should never happen
	assert(false);
}

// Starts a new GC context in which you can protect objects from being collected.
void gc_start_context() {
	GenericArray *contexts = lsvm_globals.gc_contexts;

	GCContext *context = (GCContext*)ga_create(1, sizeof(SepV), &allocator_unmanaged);
	ga_push(contexts, &context);

	log("mem", "Starting new GC context, %d contexts active.", ga_length(contexts));
}

// Ends a previously started GC context.
void gc_end_context() {
	GenericArray *contexts = lsvm_globals.gc_contexts;
	assert(ga_length(contexts) > 0);

	GCContext *last_context = *((GCContext**)ga_pop(contexts));
	ga_free((GenericArray*)last_context);

	log("mem", "Ending GC context, %d contexts active.", ga_length(contexts));
}

// Queues all roots from explicit GC contexts.
void gc_queue_gc_roots(GarbageCollection *gc) {
	// add the low-level caches which are always available
	gc_add_to_queue(gc, obj_to_sepv(lsvm_globals.module_cache));
	gc_add_to_queue(gc, obj_to_sepv(lsvm_globals.string_cache));

	// add all roots from GC contexts
	GenericArrayIterator ctx_it = ga_iterate_over(lsvm_globals.gc_contexts);
	while (!gait_end(&ctx_it)) {
		GCContext *context = *((GCContext**)gait_current(&ctx_it));

		GenericArrayIterator root_it = ga_iterate_over(&context->context_roots);
		while (!gait_end(&root_it)) {
			SepV root = *((SepV*)gait_current(&root_it));
			gc_add_to_queue(gc, root);
			gait_advance(&root_it);
		}

		gait_advance(&ctx_it);
	}
}

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

	// mark auxillary C data, if we hold any
	gc_mark_region(object->data);

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

void gc_mark_and_queue_slot(GarbageCollection *this, Slot *slot) {
	gc_add_to_queue(this, slot->value);
	// artificial slots can have special needs
	if (slot->vt->mark_and_queue)
		slot->vt->mark_and_queue(slot, this);
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
		case SEPV_TYPE_SLOT:
			gc_mark_and_queue_slot(this, (Slot*)ptr);
			break;
	}
}

// Performs the entire mark phase in one shot
void gc_mark_all(GarbageCollection *this) {
	// collect GC roots from the VM
	vm_queue_gc_roots(this);
	gc_queue_gc_roots(this);

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
//  Sweep phase - standard chunks
// ===============================================================

typedef enum MemoryBlockType {
	BLK_FREE, BLK_GARBAGE, BLK_IN_USE
} MemoryBlockType;

void gc_sweep_chunk(GarbageCollection *this, MemoryChunk *chunk) {
	alloc_unit_t *memory = chunk->memory;
	alloc_unit_t *memory_end = chunk->memory_end;
	alloc_unit_t *current_block = memory + 1;
	uint32_t units_still_in_use = 0;

	// calculate the address of the first free block (if there is one)
	alloc_unit_t *next_free;
	if (chunk->free_list->offset_to_next_free)
		next_free = memory + chunk->free_list->offset_to_next_free;
	else {
		// no free blocks right now!
		next_free = NULL;
	}

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
			uint32_t current_block_size = 0;
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

				default: {
					// never reached
					assert(false);
				}
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
			units_still_in_use += used_header->size;
			// move to next
			current_block += used_header->size;
		}
	}
	// store chunk statistics
	chunk->used = units_still_in_use;
	// fix up the last free block - mark it as the tail in the free block linked list
	last_free_block->offset_to_next_free = 0;
}


void gc_sweep_standard_chunks(GarbageCollection *this) {
	GenericArray *chunks = &this->memory->chunks;
	GenericArrayIterator it = ga_iterate_over(chunks);
	while (!gait_end(&it)) {
		MemoryChunk *chunk = *((MemoryChunk**)gait_current(&it));
		gc_sweep_chunk(this, chunk);
		gait_advance(&it);
	}
}

// ===============================================================
//  Sweep phase - outsize chunks
// ===============================================================

bool gc_outsize_chunk_in_use(OutsizeChunk *outsize_chunk) {
	return outsize_chunk->header->status.flags.marked;
}

void gc_free_outsize_chunk(OutsizeChunk *outsize_chunk) {
	debug_only(
		memset(outsize_chunk->memory, 0xEE, outsize_chunk->size);
	);
	mem_unmanaged_free(outsize_chunk->memory);
	mem_unmanaged_free(outsize_chunk);
}

void gc_sweep_outsize_chunks(GarbageCollection *this) {
	GenericArray *outsize_chunks = &this->memory->outsize_chunks;
	GenericArrayIterator it = ga_iterate_over(outsize_chunks);
	while (!gait_end(&it)) {
		OutsizeChunk *chunk = *((OutsizeChunk**)gait_current(&it));

		if (!gc_outsize_chunk_in_use(chunk)) {
			gc_free_outsize_chunk(chunk);
			gait_remove_and_advance(&it);
		} else {
			chunk->header->status.flags.marked = 0;
			gait_advance(&it);
		}
	}
}

// ===============================================================
//  Sweep phase - main
// ===============================================================

void gc_sweep_all(GarbageCollection *this) {
	log0("mem", "GC mark phase complete, starting the sweep phase.");
	gc_sweep_standard_chunks(this);
	gc_sweep_outsize_chunks(this);
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
	log("mem", "Starting a full GC, %llu/%llu bytes in use/allocated.",
			mem_used_bytes(lsvm_globals.memory), mem_allocated_bytes(lsvm_globals.memory));

	GarbageCollection *collection = gc_create();
	gc_mark_all(collection);
	gc_sweep_all(collection);
	gc_free(collection);

	// update allocated/used tallies
	mem_update_statistics();

	// allocate additional space if needed
	ManagedMemory *memory = lsvm_globals.memory;
	uint64_t total_allocated_in_std_chunks = mem_allocated_bytes(memory) - mem_allocated_outsize_chunks(memory);
	uint64_t total_free = mem_allocated_bytes(memory) - mem_used_bytes(memory);
	uint64_t required_free = total_allocated_in_std_chunks / 100.0 * GC_MINIMUM_FREE_PERCENTAGE;
	if (total_free < required_free) {
		// we're below the minimum percentage
		uint64_t chunk_size = memory->chunk_size;
		int new_blocks = (required_free - total_free + chunk_size - 1) / chunk_size;
		mem_add_chunks(new_blocks);
	}

	log("mem", "GC complete, %llu/%llu bytes in use/allocated.",
			mem_used_bytes(lsvm_globals.memory), mem_allocated_bytes(lsvm_globals.memory));
}

