/*****************************************************************
 **
 ** strings.c
 **
 **
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../common/debugging.h"
#include "mem.h"
#include "gc.h"
#include "types.h"
#include "objects.h"
#include "strings.h"
#include "runtime.h"
#include "vm.h"

// ===============================================================
//  Hashing implementation
// ===============================================================

// The current string hash implementation, Bernstein's DJB2 hash.
uint32_t cstring_hash(const char *c_string) {
	uint32_t hash = 5381;
	uint8_t *str = (uint8_t*)c_string;
	uint8_t c;
	while ((c = *str++))
		hash = ((hash << 5) + hash) ^ c;

	return hash;
}

// ===============================================================
//  Strings
// ===============================================================

SepString *sepstr_for(const char *c_string) {
	// look in the cache and returned interned string if something's there
	uint32_t hash = cstring_hash(c_string);
	if (rt.string_cache) {
		PropertyEntry *entry = props_find_entry_raw(rt.string_cache, c_string, hash);
		if (entry) {
			log("strcache", "Returning cached string: '%s'", c_string);
			return entry->name;
		}
	}

	// create and intern a new string
	SepString *string = mem_allocate(sepstr_allocation_size(c_string));
	sepstr_init(string, c_string);

	// we know the hash already
	string->hash = hash;

	// strings might be used before the string_cache is initialized
	// but if it is, intern the string
	if (rt.string_cache) {
		props_add_prop(rt.string_cache, string, &field_slot_vtable, SEPV_NOTHING);
	} else {
		// if its not, prevent it from being GC'd
		gc_register(str_to_sepv(string));
	}

	log("strcache", "Returning new string: '%s'", c_string);
	return string;
}

SepString *sepstr_new(const char *c_string) {
	// create new string, without interning
	SepString *string = mem_allocate(sepstr_allocation_size(c_string));
	sepstr_init(string, c_string);

	// register it to avoid accidentally GC'ing it away
	gc_register(str_to_sepv(string));

	return string;
}

SepString *sepstr_sprintf(const char *format, ...) {
	int buffer_size = 80;
	char *buffer = mem_unmanaged_allocate(buffer_size);
	bool fit = true;

	va_list args;
	do {
		va_start(args, format);
		int chars_written = vsnprintf(buffer, buffer_size, format, args);
		fit = chars_written < buffer_size;
		if (fit) {
			buffer[chars_written] = '\0';
		} else {
			buffer_size *= 2;
			buffer = mem_unmanaged_realloc(buffer, buffer_size);
		}
		va_end(args);
	} while (!fit);

	SepString *new_sepstr = sepstr_new(buffer);
	mem_unmanaged_free(buffer);
	return new_sepstr;
}

// Allocates a new uninitialized SepString* with a given maximum length.
SepString *sepstr_allocate(uint32_t length) {
	uint32_t size = sizeof(SepString) + length + 1;

	SepString *string = mem_allocate(size);
	string->length = length;
	string->hash = 0;

	return string;
}


SepV sepv_string(char *c_string) {
	return str_to_sepv(sepstr_for(c_string));
}

SepItem si_string(char *c_string) {
	SepItem si = {NULL, str_to_sepv(sepstr_for(c_string))};
	return si;
}

void sepstr_init(SepString *this, const char *c_string) {
	this->length = strlen(c_string);
	this->hash = 0x0;
	strcpy(this->cstr, c_string);
}

size_t sepstr_allocation_size(const char *c_string) {
	return sizeof(SepString) + strlen(c_string) + 1;
}

uint32_t sepstr_hash(SepString *this) {
	// do we have a value memorized?
	if (this->hash)
		return this->hash;

	// hash, remember and return
	return (this->hash = cstring_hash(this->cstr));
}

int sepstr_cmp(SepString *this, SepString *other) {
	// same instance (common with constants/interned)?
	if (this == other)
		return 0;
	else
		return strcmp(this->cstr, other->cstr);
}

// ===============================================================
//  SepV conversions and queries
// ===============================================================

SepString *sepv_to_str(SepV value) {
	return (SepString *)(intptr_t)(value << 3);
}

bool sepv_is_str(SepV value) {
	return (value & SEPV_TYPE_MASK) == SEPV_TYPE_STRING;
}

SepV str_to_sepv(SepString *string) {
	return SEPV_TYPE_STRING | ((intptr_t)(string) >> 3);
}
