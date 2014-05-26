/*****************************************************************
 **
 ** vm/arrays.c
 **
 ** Implementation for September arrays.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mem.h"
#include "gc.h"
#include "exceptions.h"
#include "arrays.h"

#include "../common/garray.h"
#include "../vm/runtime.h"
#include "../vm/support.h"

// ===============================================================
//  Array implementation - public
// ===============================================================

// Creates a new, empty array.
SepArray *array_create(uint32_t initial_size) {
	static ObjectTraits ARRAY_TRAITS = {REPRESENTATION_ARRAY};

	// allocate
	SepArray *array = mem_allocate(sizeof(SepArray));

	// prototypes and traits
	array->base.prototypes = obj_to_sepv(rt.Array);
	array->base.traits = ARRAY_TRAITS;

	// make sure all unallocated pointers are NULL to make sure GC
	// does not trip over some uninitialized pointers
	array->array.start = NULL;
	array->base.props.entries = NULL;
	array->base.data = NULL;

	// register as GC root to avoid collection
	gc_register(obj_to_sepv(array));

	// initialize property map (arrays don't usually hold
	// properties, so make it as small as possible)
	props_init((PropertyMap*)array, 1);

	// allocate the underlying dynamic array
	ga_init(&array->array, initial_size, sizeof(SepV), &allocator_managed);

	// return
	return array;
}

// Pushes a new value at the end of this array.
void array_push(SepArray *this, SepV value) {
	ga_push(&this->array, &value);
}

// Pushes all values from another array at the end of this array.
void array_push_all(SepArray *this, SepArray *other) {
	uint32_t initial_length = array_length(this), i;
	uint32_t other_length = array_length(other);
	array_grow(this, other_length);

	SepArrayIterator iterator = array_iterate_over(other);
	for (i = initial_length; i < initial_length + other_length; i++) {
		SepV value = arrayit_next(&iterator);
		array_set(this, i, value);
	}
}

// Pops a value from the end of this array.
SepV array_pop(SepArray *this) {
	// pop
	SepV *value_ptr = ga_pop(&this->array);

	// underflow exception?
	if (value_ptr == NULL) {
		return sepv_exception(exc.EWrongIndex,
			sepstr_for("Attempted to pop a value from an empty array."));
	}

	// return
	return *value_ptr;
}

// Gets a value at a given index.
SepV array_get(SepArray *this, uint32_t index) {
	SepV *pointer = ga_get(&this->array, index);
	if (!pointer) {
		// out of bounds!
		return sepv_exception(exc.EWrongIndex,
				sepstr_sprintf("Out of bounds access to array, index = %d", index));
	}

	// return value
	return *pointer;
}

// Sets a value at a given index.
SepV array_set(SepArray *this, uint32_t index, SepV value) {
	SepV *pointer = ga_set(&this->array, index, &value);
	if (!pointer) {
		// out of bounds!
		return sepv_exception(exc.EWrongIndex,
				sepstr_sprintf("Out of bounds access to array, index = %d", index));
	}

	// return the value
	return *pointer;
}

// Grows the array by a given number of cells.
void array_grow(SepArray *this, uint32_t cells) {
	// delegate
	ga_grow(&this->array, cells);
}

// Finds an object in the array (object identity used for equality) and returns its index, or -1 if the object is not found.
int32_t array_index_of(SepArray *this, SepV value) {
	return ga_index_of(&this->array, &value);
}

// Removes the first occurence of an object from the array (memcmp is used for comparisons) if it is present.
// Returns true if the object was present, false otherwise.
bool array_remove(SepArray *this, SepV value) {
	return ga_remove(&this->array, &value);
}

// Removes a single element from the given index.
void array_remove_at(SepArray *this, uint32_t index) {
	ga_remove_at(&this->array, index);
}

// Creates a shallow copy of the array.
SepArray *array_copy(SepArray *this) {
	SepArray *copy = array_create(array_length(this));
	array_push_all(copy, this);
	return copy;
}

// Gets the length of this array.
uint32_t array_length(SepArray *this) {
	return ga_length(&this->array);
}

// ===============================================================
//  Iteration
// ===============================================================

// Starts a new iteration over an array.
SepArrayIterator array_iterate_over(SepArray *this) {
	return ga_iterate_over(&this->array);
}

// Returns the current element under the iterator and advances the iterator itself.
SepV arrayit_next(SepArrayIterator *this) {
	SepV value = *((SepV*)gait_current(this));
	gait_advance(this);
	return value;
}

// Returns true if we have iterated over all the elements.
bool arrayit_end(SepArrayIterator *this) {
	return gait_end(this);
}


