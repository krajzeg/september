#ifndef _SEP_GARRAY_H
#define _SEP_GARRAY_H

/*****************************************************************
 **
 ** garray.h
 **
 ** Generic arrays usable with any element type, used in the
 ** implementation of the September array and the VM data stack.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdint.h>

// ===============================================================
//  Genericized arrays
// ===============================================================

typedef struct GenericArray {
	// element size in bytes
	size_t element_size;

	// pointers to the start of array, to the first unused element,
	// and to the end of allocated memory
	void *start, *end, *memory_end;
} GenericArray;

// Creates a new, empty generic array.
GenericArray *ga_create(uint32_t initial_capacity, size_t element_size);
// Initializes a new generic array in place.
void ga_init(GenericArray *this, uint32_t initial_capacity, size_t element_size);
// Pushes a new value at the end of this array.
void *ga_push(GenericArray *this, void *value_ptr);
// Pops a value from the end of this array.
void *ga_pop(GenericArray *this);
// Gets a value at a given index.
void *ga_get(GenericArray *this, uint32_t index);
// Grows the array by a given number of cells.
void *ga_set(GenericArray *this, uint32_t index, void *value_ptr);
// Sets a value at a given index.
void ga_grow(GenericArray *this, uint32_t cells);
// Gets the length of this array.
uint32_t ga_length(GenericArray *this);

/*****************************************************************/

#endif
