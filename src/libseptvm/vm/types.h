#ifndef _SEP_TYPES_H
#define _SEP_TYPES_H

/*******************************************************************/

// ===============================================================
//  Includes and pre-declarations
// ===============================================================

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct PropertyEntry;
struct SepString;
struct SepObj;
struct SepFunc;
struct Slot;

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
 * Data stack items (see below) come in a few flavors, mostly determining
 * how assignment works.
 */
typedef enum SepItemType {
	// R-values are just that - values. They have no originating slot
	// since they are usually a result of calculations and cannot
	// be assigned to.
	SIT_RVALUE = 0,
	// This type of l-value represents a property of an object, and
	// this originating object is stored. This allows assignments
	// to this l-value to go back to the right property on the
	// right object.
	SIT_PROPERTY_LVALUE = 1,
	// Artificial l-values use special slot types that have their own
	// specific rules about assignment. For example, array indexing
	// (a[2]) returns a special l-value that directs stores back
	// into the array element.
	SIT_ARTIFICIAL_LVALUE = 2
} SepItemType;

/**
 * Additional information stored for property-type l-values in order
 * to let them implement 'store' properly.
 */
typedef struct OriginInfo {
	// the object that was used on the left side of '.' to get at the property
	// - note that this object might not actually own this slot, as it could
	// belong to one of the prototypes
	SepV source;
	// the owner of the slot - can be a prototype of the host, or
	// the host itself
	SepV owner;
	// the name of the property used to access the slot
	struct SepString *property;
} OriginInfo;

/**
 * SepItem is what actually gets stored on the data stack. In addition
 * to a value represented by a SepV, it also stores the value's origin -
 * where it came from. This information is used to implement e.g the
 * assignment operator.
 */
typedef struct SepItem {
	// unassignable r-value, or one of the l-value types?
	SepItemType type;
	// the slot this value came from - provided only for l-values
	struct Slot *slot;
	// additional information used only by property-lvalues
	OriginInfo origin;
	// the value itself
	SepV value;
} SepItem;

// Creates a new r-value stack item from a SepV.
SepItem item_rvalue(SepV value);
// Creates a new property l-value stack item.
SepItem item_property_lvalue(SepV slot_owner, SepV accessed_through, struct SepString *property_name, struct Slot *slot, SepV value);
// Creates a new artificial l-value stack item - the slot has to be a standalone managed object.
SepItem item_artificial_lvalue(struct Slot *slot, SepV value);

// Retrieves the slot reference stored within the item. This is the only safe way to access it, as sometimes
// the pointer inside the struct itself might be stale and need a fix-up operation.
struct Slot* item_slot(SepItem *item);

// Checks if an item is an l-value and can be assigned to.
#define item_is_lvalue(item) (item.type != SIT_RVALUE)

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
//  Booleans and special values
// ===============================================================

SepV sepv_bool(bool truth);
SepItem si_bool(bool truth);
#define si_nothing() item_rvalue(SEPV_NOTHING)

// ===============================================================
//  Integers
// ===============================================================

typedef int64_t SepInt;


SepItem si_int(SepInt integer);

// ===============================================================
//  Type conversions and type tests
// ===============================================================

// General
#define sepv_type(value) ((value) & SEPV_TYPE_MASK)

// Pointer and non-pointer types
#define sepv_is_pointer(value) ((sepv_type(value) >= SEPV_TYPE_STRING && sepv_type(value) <= SEPV_TYPE_SLOT) || sepv_type(value) == SEPV_TYPE_EXCEPTION)
#define sepv_to_pointer(value) ((void*)(intptr_t)(value << 3))

// Generic macros for any type
#define sepv_to_typed_pointer(value, type) ((type*)(intptr_t)(value << 3))
#define pointer_to_sepv(ptr, sepv_type) ((ptr) ? ((SepV)(((intptr_t)(ptr) >> 3) | sepv_type)) : SEPV_NOTHING)
#define sepv_is(value, sepv_type) (((value) & SEPV_TYPE_MASK) == sepv_type)

// Integers
#define sepv_is_int(v) sepv_is(v, SEPV_TYPE_INT)
#define sepv_to_int(v) ((((int64_t)v) << 3) >> 3)
#define int_to_sepv(v) ((SepV)((SepInt)v) & (~SEPV_TYPE_MASK))

// Strings
#define sepv_is_str(v)  sepv_is(v, SEPV_TYPE_STRING)
#define sepv_to_str(v)  sepv_to_typed_pointer(v,struct SepString)
#define str_to_sepv(str) pointer_to_sepv(str, SEPV_TYPE_STRING)

// Functions
#define sepv_is_func(v) sepv_is(v, SEPV_TYPE_FUNC)
#define sepv_to_func(v) sepv_to_typed_pointer(v,struct SepFunc)
#define func_to_sepv(func) pointer_to_sepv(func, SEPV_TYPE_FUNC)

// Objects
#define sepv_is_obj(v) sepv_is(v, SEPV_TYPE_OBJECT)
#define sepv_to_obj(v) sepv_to_typed_pointer(v,struct SepObj)
#define obj_to_sepv(obj) pointer_to_sepv(obj, SEPV_TYPE_OBJECT)

// Slots
#define sepv_is_slot(v) sepv_is(v, SEPV_TYPE_SLOT)
#define sepv_to_slot(v) sepv_to_typed_pointer(v,struct Slot)
#define slot_to_sepv(slot) pointer_to_sepv(slot, SEPV_TYPE_SLOT)

// Exceptions
#define sepv_is_exception(v) sepv_is(v, SEPV_TYPE_EXCEPTION)

#define exception_to_obj_sepv(val) (((val) & (~SEPV_TYPE_MASK)) | SEPV_TYPE_OBJECT)
#define obj_sepv_to_exception(obj) (((val) & (~SEPV_TYPE_MASK)) | SEPV_TYPE_EXCEPTION)
#define obj_to_exception(obj) (SEPV_TYPE_EXCEPTION | (((intptr_t)obj) >> 3))
#define exception_to_obj(val) ((SepObj*)(((intptr_t)val) << 3))

/*******************************************************************/

#endif

