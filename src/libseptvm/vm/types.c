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
	SepItem item = {SIT_RVALUE, NULL, {SEPV_NOTHING, SEPV_NOTHING}, value};
	return item;
}

// Creates a new l-value stack item from a slot and its value.
SepItem item_property_lvalue(SepV slot_owner, SepV accessed_through, SepString *property_name, Slot *slot, SepV value) {
	SepItem item = {SIT_PROPERTY_LVALUE, slot, {accessed_through, slot_owner, property_name}, value};
	return item;
}

// Creates a new artificial l-value stack item - the slot has to be a standalone managed object.
SepItem item_artificial_lvalue(Slot *slot, SepV value) {
	SepItem item = {SIT_ARTIFICIAL_LVALUE, slot, {SEPV_NOTHING, SEPV_NOTHING, NULL}, value};
	return item;
}

// ===============================================================
//  Booleans
// ===============================================================

SepV sepv_bool(bool truth) {
	return truth ? SEPV_TRUE : SEPV_FALSE;
}

SepItem si_bool(bool truth) {
	return item_rvalue(truth ? SEPV_TRUE : SEPV_FALSE);
}

// ===============================================================
//  Integers
// ===============================================================

SepItem si_int(SepInt integer) {
	return item_rvalue(int_to_sepv(integer));
}
