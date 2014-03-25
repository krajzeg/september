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
