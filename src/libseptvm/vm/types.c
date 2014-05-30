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

// Retrieves the slot reference stored within the item. This is the only safe way to access it, as sometimes
// the pointer inside the struct itself might be stale and need a fix-up operation.
Slot* item_slot(SepItem *item) {
	if (item->type == SIT_PROPERTY_LVALUE) {
		// let's see if the slot still falls inside the property map of the owner
		SepObj *owner_obj = sepv_to_obj(item->origin.owner);
		void *start = owner_obj->props.entries;
		void *end = start + owner_obj->props.capacity * 2;
		void *slot = item->slot;
		if ((slot < start) || (slot >= end)) {
			// out of bounds - recalculate pointer using property information
			Slot *fixed_slot = props_find_prop(owner_obj, item->origin.property);
			item->slot = fixed_slot;
		}
	}

	// the slot is 100% correct now
	return item->slot;
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
