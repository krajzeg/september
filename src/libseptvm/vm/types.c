/*****************************************************************
 **
 ** runtime/types.c
 **
 ** Implementation for types.h - September primitive types.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/errors.h"
#include "mem.h"
#include "types.h"
#include "objects.h"

// ===============================================================
//  L-values and R-values
// ===============================================================

// Creates a new r-value stack item from a SepV.
SepItem item_rvalue(SepV value) {
	SepItem item = {NULL, value};
	return item;
}

// Creates a new l-value stack item from a slot and its value.
SepItem item_lvalue(Slot *slot, SepV value) {
	SepItem item = {slot, value};
	return item;
}

// ===============================================================
//  Booleans
// ===============================================================

SepV sepv_bool(bool truth) {
	return truth ? SEPV_TRUE : SEPV_FALSE;
}

SepItem si_bool(bool truth) {
	static SepItem si_true = {NULL, SEPV_TRUE};
	static SepItem si_false = {NULL, SEPV_FALSE};
	return truth ? si_true : si_false;
}

// ===============================================================
//  Integers
// ===============================================================

SepItem si_int(SepInt integer) {
	return item_rvalue(int_to_sepv(integer));
}

// ===============================================================
//  Strings
// ===============================================================

SepString *sepstr_create(const char *c_string) {
	SepString *string = mem_allocate(sepstr_allocation_size(c_string));
	sepstr_init(string, c_string);
	return string;
}

SepString *sepstr_sprintf(const char *format, ...) {
	int buffer_size = 80;
	char *buffer = malloc(buffer_size);
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
			buffer = realloc(buffer, buffer_size);
		}
		va_end(args);
	} while (!fit);

	SepString *new_sepstr = sepstr_create(buffer);
	free(buffer);
	return new_sepstr;
}

SepV sepv_string(char *c_string) {
	return str_to_sepv(sepstr_create(c_string));
}

SepItem si_string(char *c_string) {
	SepItem si = {NULL, str_to_sepv(sepstr_create(c_string))};
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

	// calculate hash - the current logic is Bernstein's
	// DJB2 hashing function
	uint32_t hash = 5381;
	uint8_t *str = (uint8_t*)this->cstr;
	uint8_t c;
	while ((c = *str++))
		hash = ((hash << 5) + hash) ^ c;

	// remember and return
	return this->hash = hash;
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
