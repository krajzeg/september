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
#include <stddef.h>
#include <stdbool.h>

// ===============================================================
//  Genericized arrays
// ===============================================================

typedef struct GenericArray {
	// the memory allocation strategy
	struct Allocator *allocator;

	// element size in bytes
	size_t element_size;

	// pointers to the start of array, to the first unused element,
	// and to the end of allocated memory
	void *start, *end, *memory_end;
} GenericArray;

// Creates a new, empty generic array.
GenericArray *ga_create(uint32_t initial_capacity, size_t element_size, struct Allocator *allocator);
// Initializes a new generic array in place.
void ga_init(GenericArray *this, uint32_t initial_capacity, size_t element_size, struct Allocator *allocator);

// Pushes a new value at the end of this array.
void *ga_push(GenericArray *this, void *value_ptr);
// Pops a value from the end of this array.
void *ga_pop(GenericArray *this);
// Gets a value at a given index.
void *ga_get(GenericArray *this, uint32_t index);
// Sets a value at a given index.
void *ga_set(GenericArray *this, uint32_t index, void *value_ptr);
// Grows the array by a given number of cells.
void ga_grow(GenericArray *this, uint32_t cells);
// Clears the array, truncating it to 0 entries, but keeping its storage intact.
void ga_clear(GenericArray *this);
// Gets the length of this array.
uint32_t ga_length(GenericArray *this);
// Finds an object in the array (memcmp is used for comparison) and returns its index, or -1 if the object is not found.
int32_t ga_index_of(GenericArray *this, void *value_ptr);
// Removes the first occurence of an object from the array (memcmp is used for comparisons) if it is present.
// Returns true if the object was present, false otherwise.
bool ga_remove(GenericArray *this, void *value_ptr);
// Removes a single element from the given index.
void ga_remove_at(GenericArray *this, uint32_t index);

// Frees just the internal entries table (for use with arrays created via ga_init). The memory for the array
// itself has to be freed separately.
void ga_free_entries(GenericArray *this);
// Frees the array.
void ga_free(GenericArray *this);

// ===============================================================
//  Genericized arrays
// ===============================================================

// An iterator to facilitate iterating over a generic array's elements.
typedef struct GenericArrayIterator {
	// the array
	GenericArray *array;
	// array start (used for reacting to modification from within iteration)
	void *start;
	// current position
	void *position;
} GenericArrayIterator;

// Starts a new iteration over an array.
GenericArrayIterator ga_iterate_over(GenericArray *this);
// Returns the current element under the iterator.
void *gait_current(GenericArrayIterator *this);
// Returns the index of the current element under the iterator.
uint32_t gait_index(GenericArrayIterator *this);
// Advances the iterator to the next element.
void gait_advance(GenericArrayIterator *this);
// Removes the current element and advances to the next.
void gait_remove_and_advance(GenericArrayIterator *this);
// Returns true if we have reached past the last element in the array.
bool gait_end(GenericArrayIterator *this);

// Gets the current element under the iterator, cast to the chosen type.
#define gait_current_as(iterator,type) (*((type*)gait_current(iterator)))

/*****************************************************************/

#endif
