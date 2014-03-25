#ifndef STRINGS_H_
#define STRINGS_H_

/*****************************************************************
 **
 ** strings.h
 **
 ** The September string type, SepString, and everything that
 ** pertains to handling those strings.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ===============================================================
//  The string type
// ===============================================================

/**
 * The strings type caches the length and the hashcode of the string -
 * otherwise, it's just a C string.
 */
typedef struct SepString {
	// the length of the string
	uint32_t length;
	// the hash, or just 0 if it's not cached yet
	uint32_t hash;
	// the actual C string
	char cstr[0];
} SepString;

// Extracts a string stored in a SepV.
SepString *sepv_to_str(SepV value);
// Checks whether a SepV stores a string.
bool sepv_is_str(SepV value);
// Stores a string in a SepV.
SepV str_to_sepv(SepString *string);

// Returns a SepString based on a C string. This will be a new or cached instance.
SepString *sepstr_for(const char *c_string);
// Returns a new SepString based on a C string. This will always be a new instance,
// and strings created this way never go into the interned strings cache.
SepString *sepstr_new(const char *c_string);
// Returns a new SepString obtained by running sprintf with the arguments provided.
SepString *sepstr_sprintf(const char *format, ...);
// Returns a new string, wrapped in a SepV.
SepV sepv_string(char *c_string);
// Returns a new string, wrapped in a r-value SepItem.
SepItem si_string(char *c_string);

// Determines how much memory will be needed for a SepString corresponding to a given string.
size_t sepstr_allocation_size(const char *c_string);
// Initializes a pre-allocated SepString to the string provided.
void sepstr_init(SepString *this, const char *c_string);

// Calculates the hash of a given string (and caches it inside if not cached yet).
uint32_t sepstr_hash(SepString *this);
// Compares two strings, with the return value like for strcmp (-/0/+).
int sepstr_cmp(SepString *this, SepString *other);


/*****************************************************************/

#endif
