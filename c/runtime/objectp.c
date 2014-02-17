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

#include "../vm/types.h"
#include "../vm/objects.h"
#include "../vm/exceptions.h"
#include "../vm/vm.h"
#include "support.h"

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
		or_raise_with_msg(NULL, "Incorrect expression used as property name for the ':' operator.");

	SepItem property_value = sepv_get(host_v, property_name);
	return property_value;
}

// The ':' property creation operator, valid for all objects.
SepItem object_op_colon(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;
	SepObj *host = target_as_obj(scope, &err);
		or_raise(NULL);
	SepV property_name_lv = param(scope, "property_name");

	SepV property_name_v = vm_resolve_as_literal(frame->vm, property_name_lv);
	SepString *property_name = cast_as_str(property_name_v, &err);
		or_raise_with_msg(NULL, "Incorrect expression used as property name for the ':' operator.");

	// check if it already exists
	if (props_find_prop(host, property_name)) {
		raise(NULL, "Property '%s' cannot be created because it already exists.", sepstr_to_cstr(property_name));
	}

	// create the field
	props_accept_prop(host, property_name, field_create(SEPV_NOTHING));

	// return the slot for reassigning (reacquire it as it could have been moved
	// in memory)
	Slot *slot = props_find_prop(host, property_name);
	return item_lvalue(slot, SEPV_NOTHING);
}

// ===============================================================
//  Stringifying things
// ===============================================================

SepItem object_debug_string(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;
	SepString *debug_str = sepv_debug_string(target(scope), &err);
		or_raise(NULL);
	return item_rvalue(str_to_sepv(debug_str));
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
	// -- property access
	obj_add_builtin_method(Object, ".", object_op_dot, 1, "?property_name");
	obj_add_builtin_method(Object, ":", object_op_colon, 1, "?property_name");

	// add common methods
	obj_add_builtin_method(Object, "debugString", object_debug_string, 0);

	return Object;
}
