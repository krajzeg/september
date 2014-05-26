#ifndef _SEP_ARRAYS_H
#define _SEP_ARRAYS_H

/*****************************************************************
 **
 ** vm/arrays.h
 **
 ** Contains everything needed for September arrays.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdint.h>
#include <stddef.h>
#include "../common/garray.h"
#include "types.h"
#include "objects.h"

// ===============================================================
//  Arrays
// ===============================================================

/**
 * September arrays are an extension of September objects, so they
 * start with a SepObj struct and all obj_* methods can apply also
 * to them. The array functionality is an 'extra' capability on
 * top of that.
 */
typedef struct SepArray {
	// the SepObj base struct
	SepObj base;
	// the actual backing array
	GenericArray array;
} SepArray;

// Creates a new, empty array.
SepArray *array_create(uint32_t initial_size);
// Pushes a new value at the end of this array.
void array_push(SepArray *this, SepV value);
// Pops a value from the end of this array.
SepV array_pop(SepArray *this);
// Gets a value at a given index.
SepV array_get(SepArray *this, uint32_t index);
// Grows the array by a given number of cells.
void array_grow(SepArray *this, uint32_t cells);
// Sets a value at a given index.
SepV array_set(SepArray *this, uint32_t index, SepV value);
// Gets the length of this array.
uint32_t array_length(SepArray *this);
// Finds an object in the array (object identity used for equality) and returns its index, or -1 if the object is not found.
int32_t array_index_of(SepArray *this, SepV value);
// Removes the first occurence of an object from the array (memcmp is used for comparisons) if it is present.
// Returns true if the object was present, false otherwise.
bool array_remove(SepArray *this, SepV value);
// Removes a single element from the given index.
void array_remove_at(SepArray *this, uint32_t index);
// Creates a shallow copy of the array.
SepArray *array_copy(SepArray *this);
// Pushes all values from another array at the end of this array.
void array_push_all(SepArray *this, SepArray *other);

// ===============================================================
//  Iteration
// ===============================================================

// Iterator for a SepArray is just a generic iterator, nothing special.
typedef GenericArrayIterator SepArrayIterator;

// Starts a new iteration over an array.
SepArrayIterator array_iterate_over(SepArray *this);
// Returns the current element under the iterator and advances the iterator itself.
SepV arrayit_next(SepArrayIterator *this);
// Returns true if we have iterated over all the elements.
bool arrayit_end(SepArrayIterator *this);

/*****************************************************************/

#endif
