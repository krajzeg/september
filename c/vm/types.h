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
#define SEPV_TYPE_MASK    (7ull << 61)
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

// ===============================================================
//  Integers
// ===============================================================

typedef int64_t SepInt;

#define sepv_is_int(v) (((v) & SEPV_TYPE_MASK) == SEPV_TYPE_INT)
#define sepv_to_int(v) ((SepInt)(((v) << 3) >> 3))
#define int_to_sepv(v) ((SepV)((SepInt)v) &~ SEPV_TYPE_MASK)

SepItem si_int(SepInt integer);

// ===============================================================
//  Strings
// ===============================================================

typedef struct SepString {
	uint32_t length;
	uint32_t hash;
	char content[0];
} SepString;

SepString *sepv_to_str(SepV value);
bool sepv_is_str(SepV value);
SepV str_to_sepv(SepString *string);

SepV sepv_string(const char *c_string);
SepString *sepstr_create(const char *c_string);
SepString *sepstr_sprintf(const char *format, ...);

size_t sepstr_allocation_size(const char *c_string);
void sepstr_init(SepString *this, const char *c_string);

uint32_t sepstr_hash(SepString *this);
int sepstr_cmp(SepString *this, SepString *other);
const char *sepstr_to_cstr(SepString *sstring);

/*******************************************************************/

#endif
