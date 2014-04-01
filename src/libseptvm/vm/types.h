#ifndef _SEP_TYPES_H
#define _SEP_TYPES_H

/*******************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*******************************************************************/

// ===============================================================
//  Basic types for representing data.
// ===============================================================

/**
 * SepV is the 'Any' type used by September. You can store any type
 * of value directly available in September in a SepV. The top 3 bits
 * signify the type (see below for the 8 types available), while the
 * rest defines the actual value and is type-dependent.
 */

typedef uint64_t SepV;

/**
 * SepItem is what actually gets stored on the data stack. In addition
 * to a value represented by a SepV, it also stores the value's origin -
 * the slot it came from. This slot can be used to implement things
 * like the assignment operator. The 'origin' is NULL in case of
 * r-values which don't come from a slot.
 */
typedef struct SepItem {
	// the slot this value came from
	struct Slot *origin;
	// the value itself
	SepV value;
} SepItem;

// Creates a new r-value stack item from a SepV.
SepItem item_rvalue(SepV value);
// Creates a new l-value stack item from a slot and its value.
SepItem item_lvalue(struct Slot *slot, SepV value);
// Checks if an item is an l-value and can be assigned to.
#define item_is_lvalue(item) (item.slot != NULL)

// ===============================================================
//  SepV type-related constants
// ===============================================================

// mask for the type bits
#define SEPV_TYPE_MASK    (0xe000000000000000ull)
// mask for the value bits
#define SEPV_VALUE_MASK   (0x1fffffffffffffffull)

// integers: 61-bit signed integers
#define SEPV_TYPE_INT     (0ull << 61)

// floats: truncated IEEE 64-bit double precision floats
// (3 least significant digits lost)
#define SEPV_TYPE_FLOAT   (1ull << 61)

// strings: shifted pointer to a SepStr
#define SEPV_TYPE_STRING  (2ull << 61)

// objects: shifted pointer to a SepObj
// arrays are descended from SepObj and also use this type
#define SEPV_TYPE_OBJECT  (3ull << 61)

// functions: shifted pointer to a SepFunc
#define SEPV_TYPE_FUNC    (4ull << 61)

// slots: shifted pointer to a Slot
#define SEPV_TYPE_SLOT    (5ull << 61)

// special constants: like Nothing, True, False
#define SEPV_TYPE_SPECIAL (6ull << 61)

// exceptions: this type is only used for already thrown,
// live exceptions. This type on a return value from a function
// means that the exception was thrown from within. Should a function
// want to just return an exception, it would do it through a
// value with the type OBJECT.
#define SEPV_TYPE_EXCEPTION (7ull << 61)

// ===============================================================
//  Special values
// ===============================================================

/**
 * All the special constants that go into SEPV_TYPE_SPECIAL.
 */

// the 'null' of September
#define SEPV_NOTHING (SEPV_TYPE_SPECIAL | 0x00)
// boolean values
#define SEPV_FALSE   (SEPV_TYPE_SPECIAL | 0x01)
#define SEPV_TRUE    (SEPV_TYPE_SPECIAL | 0x02)
// a special marker that ends every argument list
#define SEPV_END_ARGUMENTS (SEPV_TYPE_SPECIAL | 0x03)
// a special object that possesses all possible properties, and
// the value of each property is simply its name
#define SEPV_LITERALS (SEPV_TYPE_SPECIAL | 0x04)
// a special marker pushed on the stack to mark the place to which
// the data stack has to be cleared in case of an exception being
// thrown
#define SEPV_UNWIND_MARKER (SEPV_TYPE_SPECIAL | 0x05)
// return value used by the "break" function to signify breaking
// out of the loop
#define SEPV_BREAK (SEPV_TYPE_SPECIAL | 0x06)
// an internal special value used to signify "no SEPV was passed in"
// different than SEPV_NOTHING, as Nothing is a real object that
// can be passed to a function (unlike SEPV_NO_VALUE).
#define SEPV_NO_VALUE (SEPV_TYPE_SPECIAL | 0x07)

// ===============================================================
//  Working with SepV types
// ===============================================================

#define sepv_type(value) ((value) & SEPV_TYPE_MASK)
#define sepv_is_pointer(value) ((sepv_type(value) >= SEPV_TYPE_STRING && sepv_type(value) <= SEPV_TYPE_SLOT) || sepv_type(value) == SEPV_TYPE_EXCEPTION)
#define sepv_to_pointer(value) ((void*)(intptr_t)(value << 3))

// ===============================================================
//  Booleans and special values
// ===============================================================

SepV sepv_bool(bool truth);
SepItem si_bool(bool truth);
#define si_nothing() item_rvalue(SEPV_NOTHING);

// ===============================================================
//  Integers
// ===============================================================

typedef int64_t SepInt;

#define sepv_is_int(v) (((v) & SEPV_TYPE_MASK) == SEPV_TYPE_INT)
#define sepv_to_int(v) ((((int64_t)v) << 3) >> 3)
#define int_to_sepv(v) ((SepV)((SepInt)v) & (~SEPV_TYPE_MASK))

SepItem si_int(SepInt integer);

/*******************************************************************/

#endif

