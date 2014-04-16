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

#include <septvm.h>

// ===============================================================
//  Common operators
// ===============================================================

// The '.' property access operator, valid for all objects.
SepItem object_op_dot(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;
	SepV host_v = target(scope);
	SepV property_name_lv = param(scope, "property_name");
	SepV property_name_v = vm_resolve_as_literal(frame->vm, property_name_lv);
	SepString *property_name = cast_as_str(property_name_v, &err);
		or_raise_with_msg(exc.EWrongType, "Incorrect expression used as property name for the '.' operator.");

	SepItem property_value = sepv_get_item(host_v, property_name);
	return property_value;
}

SepItem insert_slot_impl(SepObj *scope, ExecutionFrame *frame, SlotType *slot_type, SepV value) {
	SepError err = NO_ERROR;
	SepV host_v = target(scope);
	SepObj *host = target_as_obj(scope, &err);
		or_raise(exc.EWrongType);
	SepV property_name_lv = param(scope, "property_name");

	SepV property_name_v = vm_resolve_as_literal(frame->vm, property_name_lv);
	SepString *property_name = cast_as_str(property_name_v, &err);
		or_raise_with_msg(exc.EWrongType, "Incorrect expression used as property name for the ':' operator.");

	// check if it already exists
	if (props_find_prop(host, property_name))
		raise(exc.EPropertyAlreadyExists, "Property '%s' cannot be created because it already exists.", property_name->cstr);

	// create the slot
	Slot *slot = props_add_prop(host, property_name, slot_type, value);
	return item_property_lvalue(host_v, host_v, slot, SEPV_NOTHING);
}

// The ':' field creation operator, valid for all objects.
SepItem object_op_colon(SepObj *scope, ExecutionFrame *frame) {
	return insert_slot_impl(scope, frame, &st_field, SEPV_NOTHING);
}

// The '::' method creation operator, valid for all objects.
SepItem object_op_double_colon(SepObj *scope, ExecutionFrame *frame) {
	return insert_slot_impl(scope, frame, &st_method, SEPV_NOTHING);
}

// ===============================================================
//  Common operations
// ===============================================================

// Object.resolve()
// Used for resolving a lazy parameter. If the object is a function created for
// lazy evaluation, it will be evaluated. Otherwise, the object itself is returned
// unchanged. This ensures safe treatment of lazy parameters.
SepItem object_resolve(SepObj *scope, ExecutionFrame *frame) {
	SepV target = target(scope);
	return item_rvalue(vm_resolve(frame->vm, target));
}

// Object.instantiate()
// Used by the class system. Creates a new object and sets its prototype to the object
// on which the instantiate() method was invoked.
SepItem object_instantiate(SepObj *scope, ExecutionFrame *frame) {
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
	Object->prototypes = SEPV_NOTHING;

	// add operators common to all objects
	obj_add_builtin_method(Object, ".", object_op_dot, 1, "?property_name");
	obj_add_builtin_method(Object, ":", object_op_colon, 1, "?property_name");
	obj_add_builtin_method(Object, "::", object_op_double_colon, 1, "?property_name");

	// add common methods
	obj_add_builtin_method(Object, "resolve", object_resolve, 0);
	obj_add_builtin_method(Object, "instantiate", object_instantiate, 0);
	obj_add_builtin_method(Object, "is", object_is, 1, "desired_class");

	return Object;
}
