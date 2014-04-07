/*****************************************************************
 **
 ** garray.c
 **
 ** Implementation for a generic dynamic array.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "garray.h"
#include "../vm/mem.h"

// ===============================================================
//  Generic array implementation - privates
// ===============================================================

// how much the array grows each time it runs out of storage
#define ARRAY_GROWTH_FACTOR 1.5f

// Resizes the array's backing store to a new number of elements.
void _ga_resize(GenericArray *this, uint32_t new_size) {
	// create new content array, copying the old contents over
	void *new_contents = this->allocator->reallocate(this->start, this->end - this->start, new_size * this->element_size);

	// attach new storage
	ptrdiff_t length = this->end - this->start;
	this->start = new_contents;
	this->end = new_contents + length;
	this->memory_end = new_contents + (new_size * this->element_size);
}

// ===============================================================
//  Array implementation - public
// ===============================================================

// Creates a new, empty array.
GenericArray *ga_create(uint32_t initial_capacity, size_t element_size, Allocator *allocator) {
	// allocate, initialize, return
	GenericArray *array = allocator->allocate(sizeof(GenericArray));
	ga_init(array, initial_capacity, element_size, allocator);
	return array;
}

// Initializes a new generic array in place.
void ga_init(GenericArray *this, uint32_t initial_capacity, size_t element_size, Allocator *allocator) {
	this->allocator = allocator;
	this->element_size = element_size;
	// allocate some storage for contents
	this->start = allocator->allocate(initial_capacity * element_size);
	this->end = this->start;
	this->memory_end = this->start + (initial_capacity * element_size);
}

// Pushes a new value at the end of this array.
void *ga_push(GenericArray *this, void *value) {
	// out of space?
	if (this->end == this->memory_end) {
		uint32_t current_size = (this->end - this->start) / this->element_size;
		uint32_t new_size = (uint32_t)(current_size * ARRAY_GROWTH_FACTOR) + 1;
		_ga_resize(this, new_size);
	}

	// push
	memcpy(this->end, value, this->element_size);
	this->end += this->element_size;
	return this->end - this->element_size;
}

// Pops a value from the end of this array.
void *ga_pop(GenericArray *this) {
	// detect underflow
	if (this->end == this->start) {
		return NULL;
	}

	// pop
	this->end -= this->element_size;
	return this->end;
}

// Gets a value at a given index.
void *ga_get(GenericArray *this, uint32_t index) {
	void *pointer = this->start + this->element_size * index;

	// detect out of bounds
	if (pointer >= this->end)
		return NULL;

	// return value
	return pointer;
}

// Sets a value at a given index.
void *ga_set(GenericArray *this, uint32_t index, void *value) {
	void *pointer = this->start + this->element_size * index;
	if (pointer >= this->end)
		return NULL;

	// set the value and return it
	memcpy(pointer, value, this->element_size);
	return pointer;
}

// Grows the array by a given number of cells.
void ga_grow(GenericArray *this, uint32_t cells) {
	// resize storage if needed
	if (this->end + (cells * this->element_size) >= this->memory_end) {
		uint32_t current_size = (this->end - this->start) / this->element_size;
		uint32_t new_size = (uint32_t)(current_size * ARRAY_GROWTH_FACTOR) + cells;
		_ga_resize(this, new_size);
	}

	// move pointer to make the cells appear
	this->end += cells * this->element_size;
}

// Clears the array, truncating it to 0 entries, but keeping its storage intact.
void ga_clear(GenericArray *this) {
	this->end = this->start;
}

// Gets the length of this array.
uint32_t ga_length(GenericArray *this) {
	return (this->end - this->start) / this->element_size;
}

// Finds an object in the array (memcmp is used for comparison) and returns its index, or -1 if the object is not found.
int32_t ga_index_of(GenericArray *this, void *needed_ptr) {
	GenericArrayIterator it = ga_iterate_over(this);
	while (!gait_end(&it)) {
		void *element_ptr = gait_current(&it);
		if (memcmp(needed_ptr, element_ptr, this->element_size) == 0)
			return gait_index(&it);
		else
			gait_advance(&it);
	}

	// nothing found
	return -1;
}

// Removes the first occurence of an object from the array (memcmp is used for comparisons) if it is present.
// Returns true if the object was present, false otherwise.
bool ga_remove(GenericArray *this, void *value_ptr) {
	int32_t index = ga_index_of(this, value_ptr);
	if (index == -1)
		return false;

	// copy all elements following the one to be removed one index to the left
	void *location = this->start + index * this->element_size;
	memmove(location, location + this->element_size, this->end - location - this->element_size);
	// shorten the array by one element
	this->end -= this->element_size;
	// we have removed something
	return true;
}

// ===============================================================
//  Iteration
// ===============================================================

// Starts a new iteration over an array.
GenericArrayIterator ga_iterate_over(GenericArray *this) {
	GenericArrayIterator it = {this, this->start, this->start};
	return it;
}

// Returns the current element under the iterator.
void *gait_current(GenericArrayIterator *this) {
	return this->position;
}

// Returns the index of the current element under the iterator.
uint32_t gait_index(GenericArrayIterator *this) {
	return (this->position - this->start) / this->array->element_size;
}

// Advances the iterator to the next element.
void gait_advance(GenericArrayIterator *this) {
	this->position += this->array->element_size;
}

// Returns true if we have reached past the last element in the array.
bool gait_end(GenericArrayIterator *this) {
	return this->position >= this->array->end;
}

// ===============================================================
//  Freeing memory
// ===============================================================

// Frees just the internal entries table (for use with arrays created via ga_init). The memory for the array
// itself has to be freed separately.
void ga_free_entries(GenericArray *this) {
	this->allocator->free(this->start);
}

// Frees the array.
void ga_free(GenericArray *this) {
	this->allocator->free(this->start);
	this->allocator->free(this);
}
