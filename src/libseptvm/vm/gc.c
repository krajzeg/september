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
//  Mark queue
// ===============================================================

SepV gc_next_in_queue(GarbageCollection *this) {
	// are we done yet?
	if (this->queue_start == this->queue_end)
		return SEPV_NO_VALUE;

	// no - get first object in queue
	SepV object = *((SepV*)ga_get(&this->mark_queue, this->queue_start));
	this->queue_start = (this->queue_start + 1) % ga_length(&this->mark_queue);
	return object;
}


void gc_add_to_queue(GarbageCollection *this, SepV object) {
	// non-pointer types do not use managed memory
	if (!sepv_is_pointer(object))
		return;
	// already marked?
	if (used_block_header(sepv_to_pointer(object))->status.flags.marked)
		return;

	// add to the queue buffer
	uint32_t buffer_length = ga_length(&this->mark_queue);
	bool buffer_full = (buffer_length == 0) || ((this->queue_end + 1) % buffer_length) == this->queue_start;
	if (!buffer_full) {
		// buffer not full yet, add at end
		ga_set(&this->mark_queue, this->queue_end, &object);
		this->queue_end = (this->queue_end + 1) % buffer_length;
	} else {
		// buffer full - expand the buffer
		// this does not insert at the end of the queue, but the order
		// doesn't really matter
		ga_push(&this->mark_queue, &object);
	}
}

// ===============================================================
//  Mark phase
// ===============================================================

// Marks a region of memory.
void gc_mark_region(void *region) {
	used_block_header(region)->status.flags.marked = 1;
}

// Queues objects reachable from a SepObj for marking and marks its internal
// memory regions.
void gc_mark_and_queue_obj(GarbageCollection *this, SepObj *object) {
	// mark the property map region
	gc_mark_region(object->props.entries);

	// queue property values
	PropertyIterator it = props_iterate_over(object);
	while (!propit_end(&it)) {
		// make sure the name of the property is not GC'd
		gc_mark_region(propit_name(&it));
		// the value will wait its turn in the queue
		gc_add_to_queue(this, propit_value(&it));
	}

	// queue prototypes
	gc_add_to_queue(this, object->prototypes);

	// arrays need to collect their elements too
	if (object->traits.representation == REPRESENTATION_ARRAY) {
		SepArrayIterator ait = array_iterate_over((SepArray*)object);
		while (!arrayit_end(&ait)) {
			gc_add_to_queue(this, arrayit_next(&ait));
		}
	}
}

// Queues objects reachable from a SepFunc for marking and marks its internal
// memory regions.
void gc_mark_and_queue_func(GarbageCollection *this, SepFunc *func) {
	// mark the region storing parameters of this function
	gc_mark_region(func->vt->get_parameters(func));
	// add referenced scopes
	gc_add_to_queue(this, func->vt->get_declaration_scope(func));
	gc_add_to_queue(this, func->vt->get_this_pointer(func));
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

	int root_count = this->queue_end - this->queue_start;
	if (root_count < 0)
		root_count += ga_length(&this->mark_queue);
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

void gc_sweep_chunk(GarbageCollection *this, MemoryChunk *chunk) {
	// to be implemented
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

	gc->memory = _managed_memory;
	ga_init(&gc->mark_queue, 32, sizeof(SepV), &allocator_unmanaged);
	gc->queue_start = gc->queue_end = 0;

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

