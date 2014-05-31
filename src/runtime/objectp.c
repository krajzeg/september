/*****************************************************************
 **
 ** objectp.c
 **
 **
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "common.h"

// ===============================================================
//  Common operators
// ===============================================================

// The '.' property access operator, valid for all objects.
SepItem object_op_dot(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepV host_v = target(scope);
	SepV property_name_lv = param(scope, "property_name");
	SepV property_name_v = vm_resolve_as_literal(frame->vm, property_name_lv);
	SepString *property_name = cast_as_str(property_name_v, &err);
		or_raise(err);

	SepItem property_value = sepv_get_item(host_v, property_name);
	return property_value;
}

// Indexing - very similar to the '.' operator, but the property name is eager.
SepItem object_op_index(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepV host_v = target(scope);
	SepString *property_name = cast_as_named_str("Property name", param(scope, "property_name"), &err);
		or_raise(err);
	return sepv_get_item(host_v, property_name);
}

// Base function used to implement '::' and ':::'.
SepItem insert_slot_impl(SepObj *scope, ExecutionFrame *frame, SlotType *slot_type, SepV value) {
	SepV err = SEPV_NOTHING;
	SepV host_v = target(scope);
	SepObj *host = target_as_obj(scope, &err);
		or_raise(err);
	SepV property_name_lv = param(scope, "property_name");

	SepV property_name_v = vm_resolve_as_literal(frame->vm, property_name_lv);
	SepString *property_name = cast_as_str(property_name_v, &err);
		or_raise(err);

	// create or reassign the slot
	Slot *slot = props_add_prop(host, property_name, slot_type, value);
	return item_property_lvalue(host_v, host_v, property_name, slot, SEPV_NOTHING);
}

// The '::' field creation operator, valid for all objects.
SepItem object_op_double_colon(SepObj *scope, ExecutionFrame *frame) {
	return insert_slot_impl(scope, frame, &st_field, SEPV_NOTHING);
}

// The ':::' method creation operator, valid for all objects.
SepItem object_op_triple_colon(SepObj *scope, ExecutionFrame *frame) {
	return insert_slot_impl(scope, frame, &st_method, SEPV_NOTHING);
}

// Simple identity equality common to all objects.
SepItem object_op_equals(SepObj *scope, ExecutionFrame *frame) {
	SepV target = target(scope);
	SepV other = param(scope, "other");
	return si_bool(target == other);
}

// ===============================================================
//  Common operations
// ===============================================================

// Object.accept(property_name, slot)
// Allows to add an arbitrary property to an object, with any slot type.
// The resulting value is returned, with the slot set - so this can be
// used as an l-value.
SepItem object_accept(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepObj *target = target_as_obj(scope, &err);
		or_raise(err);
	SepString *name = param_as_str(scope, "property_name", &err);
		or_raise(err);
	SepV slot_v = param(scope, "slot");
	if (!sepv_is_slot(slot_v))
		raise(exc.EWrongType, "Only slots can be accepted into objects.");

	Slot *slot = sepv_to_slot(slot_v);
	slot = props_accept_prop(target, name, slot);
	return sepv_get_item(obj_to_sepv(target), name);
}

// Object.resolve(scope = None)
// Used for resolving a lazy parameter. If the object is a function created for
// lazy evaluation, it will be evaluated. Otherwise, the object itself is returned
// unchanged. This ensures safe treatment of lazy parameters.
// Optionally, the resolution scope can be changed to obtain special effects
// such as resolving as literal identifier.
SepItem object_resolve(SepObj *scope, ExecutionFrame *frame) {
	SepV target = target(scope);
	SepV resolution_scope = param(scope, "scope");
	if (resolution_scope != SEPV_NO_VALUE) {
		return item_rvalue(vm_resolve_in(frame->vm, target, resolution_scope));
	} else {
		return item_rvalue(vm_resolve(frame->vm, target));
	}
}

// Object.spawn()
// Used by the class system. Creates a new object and sets its prototype to the object
// on which the spawn() method was invoked.
SepItem object_spawn(SepObj *scope, ExecutionFrame *frame) {
	SepV prototype = target(scope);
	return si_obj(obj_create_with_proto(prototype));
}

// Object.is(desired_class)
// Checks whether the object belongs to a class given as parameter.
SepItem object_is(SepObj *scope, ExecutionFrame *frame) {
	SepV target = target(scope);
	SepV actual_class = sepv_lenient_get(target, sepstr_for("<class>"));
	if (actual_class != SEPV_NO_VALUE) {
		// is it the thing we're looking for?
		SepV desired_class = param(scope, "desired_class");

		// loop over all superclasses
		while (true) {
			// found it?
			if (desired_class == actual_class)
				return si_bool(true);

			// nope, let's see if we have a further superclass
			actual_class = sepv_lenient_get(actual_class, sepstr_for("<superclass>"));
			if (actual_class == SEPV_NO_VALUE || actual_class == SEPV_NOTHING) {
				// no, we don't - we haven't found the desired class anywhere
				// in the inheritance chain
				return si_bool(false);
			}
		}
	} else {
		// no class slot, no inheritance, no being anything
		return si_bool(false);
	}
}

// ===============================================================
//  Object prototype
// ===============================================================

// Creates the Object class object.
SepObj *create_object_prototype() {
	// create Object class
	SepObj *Object = make_class("Object", NULL);
	obj_set_prototypes(Object, SEPV_NOTHING);

	// add operators common to all objects
	obj_add_builtin_method(Object, ".", object_op_dot, 1, "?property_name");
	obj_add_builtin_method(Object, "::", object_op_double_colon, 1, "?property_name");
	obj_add_builtin_method(Object, ":::", object_op_triple_colon, 1, "?property_name");
	obj_add_builtin_method(Object, "[]", object_op_index, 1, "property_name");
	obj_add_builtin_method(Object, "==", object_op_equals, 1, "other");

	// add common methods
	obj_add_builtin_method(Object, "resolve", object_resolve, 1, "=scope");
	obj_add_builtin_method(Object, "accept", object_accept, 2, "property_name", "slot");
	obj_add_builtin_method(Object, "spawn", object_spawn, 0);
	obj_add_builtin_method(Object, "is", object_is, 1, "desired_class");

	// add special properties
	obj_add_slot(Object, "prototypes", &st_prototype_list, SEPV_NO_VALUE);

	return Object;
}
