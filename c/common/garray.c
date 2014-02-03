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
	void *new_contents = mem_allocate(new_size * this->element_size);
	memcpy(new_contents, this->start, this->end - this->start);

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
GenericArray *ga_create(uint32_t initial_capacity, size_t element_size) {
	// allocate, initialize, return
	GenericArray *array = mem_allocate(sizeof(GenericArray));
	ga_init(array, initial_capacity, element_size);
	return array;
}

// Initializes a new generic array in place.
void ga_init(GenericArray *this, uint32_t initial_capacity, size_t element_size) {
	// remember element size
	this->element_size = element_size;
	// allocate some storage for contents
	this->start = mem_allocate(initial_capacity * element_size);
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

// Gets the length of this array.
uint32_t ga_length(GenericArray *this) {
	return (this->end - this->start) / this->element_size;
}
