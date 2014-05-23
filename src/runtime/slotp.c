/*****************************************************************
 **
 ** slotp.c
 **
 ** The Slot prototype implementation, including all static methods
 ** for creating new slots.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <septvm.h>

// ===============================================================
//  Slot creation
// ===============================================================

SepItem slot_creation_impl(SepObj *scope, ExecutionFrame *frame, SlotType *type) {
	SepV initial_value = param(scope, "initial_value");
	if (initial_value == SEPV_NO_VALUE)
		initial_value = SEPV_NOTHING;
	SepV slot = slot_to_sepv(slot_create(type, initial_value));
	return item_rvalue(slot);
}

SepItem slot_field(SepObj *scope, ExecutionFrame *frame) {
	return slot_creation_impl(scope, frame, &st_field);
}

SepItem slot_method(SepObj *scope, ExecutionFrame *frame) {
	return slot_creation_impl(scope, frame, &st_method);
}

SepItem slot_magicword(SepObj *scope, ExecutionFrame *frame) {
	return slot_creation_impl(scope, frame, &st_magic_word);
}

// ===============================================================
//  Slot prototype
// ===============================================================

SepObj *create_slot_prototype() {
	SepObj *Slot = make_class("Slot", NULL);

	obj_add_builtin_method(Slot, "field", &slot_field, 1, "=initial_value");
	obj_add_builtin_method(Slot, "method", &slot_method, 1, "=initial_value");
	obj_add_builtin_method(Slot, "magicWord", &slot_magicword, 1, "=initial_value");

	return Slot;
}
