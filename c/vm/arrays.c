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
#include "../common/garray.h"
#include "mem.h"
#include "exceptions.h"
#include "arrays.h"

// ===============================================================
//  Array implementation - public
// ===============================================================

// Creates a new, empty array.
SepArray *array_create(uint32_t initial_size) {
	static ObjectTraits ARRAY_TRAITS = {REPRESENTATION_ARRAY};

	// allocate
	SepArray *array = mem_allocate(sizeof(SepArray));

	// initialize property map (arrays don't usually hold
	// properties, so make it as small as possible)
	props_init((PropertyMap*)array, 1);

	// prototypes and traits
	array->base.prototypes = SEPV_NOTHING;
	array->base.traits = ARRAY_TRAITS;

	// allocate the underlying dynamic array
	ga_init(&array->array, initial_size, sizeof(SepV));

	// return
	return array;
}

// Pushes a new value at the end of this array.
void array_push(SepArray *this, SepV value) {
	ga_push(&this->array, &value);
}

// Pops a value from the end of this array.
SepV array_pop(SepArray *this) {
	// pop
	SepV *value_ptr = ga_pop(&this->array);

	// underflow exception?
	if (value_ptr == NULL) {
		return sepv_exception(NULL,
			sepstr_create("Attempted to pop a value from an empty array."));
	}

	// return
	return *value_ptr;
}

// Gets a value at a given index.
SepV array_get(SepArray *this, uint32_t index) {
	SepV *pointer = ga_get(&this->array, index);
	if (!pointer) {
		// out of bounds!
		return sepv_exception(NULL,
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
		return sepv_exception(NULL,
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

// Gets the length of this array.
uint32_t array_length(SepArray *this) {
	return ga_length(&this->array);
}

