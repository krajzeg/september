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

/*****************************************************************/

#endif
