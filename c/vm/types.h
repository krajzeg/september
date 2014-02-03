#ifndef _SEP_TYPES_H
#define _SEP_TYPES_H

/*******************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*******************************************************************/

/**
 * SepV is the 'Any' type used by September. You can store any type
 * of value directly available in September in a SepV. The top 3 bits
 * signify the type (see below for the 8 types available), while the
 * rest controls the actual value.
 */

typedef uint64_t SepV;

// mask for the type bits
#define SEPV_TYPE_MASK    (7ull << 61)
// mask for the value bits
#define SEPV_VALUE_MASK   (0x1fffffffffffffffull)

// integers: 61-bit signed integers
#define SEPV_TYPE_INT     (0ull << 61)

// floats: truncated 64-bit double precision floats
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
// means that an exception was thrown. Should a function just
// return an exception, it would do it through an OBJECT type
// value.
#define SEPV_TYPE_EXCEPTION (7ull << 61)

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
