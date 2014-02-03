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
#include "objectp.h"

// ===============================================================
//  Common operators
// ===============================================================

// The '.' property access operator, valid for all objects.
SepItem object_op_dot(SepObj *p, ExecutionFrame *frame) {
	SepV host = props_get_prop(p, sepstr_create("this"));
	SepV property_v = props_get_prop(p, sepstr_create("property_name"));
	if (!sepv_is_func(property_v)) {
		return si_exception(NULL,
				sepstr_sprintf("Incorrect expression used as property name for the '.' operator."));
	}
	SepV property_name_v = vm_resolve_as_literal(frame->vm, sepv_to_func(property_v));
	if (!sepv_is_str(property_name_v)) {
		return si_exception(NULL,
				sepstr_sprintf("Incorrect expression used as property name for the '.' operator."));
	}

	SepItem property_value = sepv_get(host, sepv_to_str(property_name_v));
	return property_value;
}

// The ':' property creation operator, valid for all objects.
SepItem object_op_colon(SepObj *p, ExecutionFrame *frame) {
	SepV host_v = props_get_prop(p, sepstr_create("this"));
	if (!sepv_is_obj(host_v)) {
		return si_exception(NULL,
				sepstr_sprintf("New properties cannot be created on non-objects."));
	}
	SepObj *host = sepv_to_obj(host_v);

	SepV property_v = props_get_prop(p, sepstr_create("property_name"));
	if (!sepv_is_func(property_v)) {
		return si_exception(NULL,
				sepstr_sprintf("Incorrect expression used as property name for the ':' operator."));
	}
	SepV property_name_v = vm_resolve_as_literal(frame->vm, sepv_to_func(property_v));
	if (!sepv_is_str(property_name_v)) {
		return si_exception(NULL,
				sepstr_sprintf("Incorrect expression used as property name for the ':' operator."));
	}

	SepString *property = sepv_to_str(property_name_v);

	// check if it already exists
	if (props_find_prop(host, property)) {
		return si_exception(NULL,
				sepstr_sprintf("Property '%s' cannot be created because it already exists.", sepstr_to_cstr(property)));
	}

	// create the field
	props_accept_prop(host, property, field_create(SEPV_NOTHING));

	// return the slot for reassigning (reacquire it as it could have been moved
	// in memory)
	Slot *slot = props_find_prop(host, property);
	return item_lvalue(slot, SEPV_NOTHING);
}



// ===============================================================
//  Object prototype
// ===============================================================

// Creates the Object class object.
SepObj *create_object_prototype() {
	SepObj *Object = obj_create_with_proto(SEPV_NOTHING);

	SepV dot = func_to_sepv(builtin_create(object_op_dot, 1, "?property_name"));
	props_accept_prop(Object, sepstr_create("."), method_create(dot));
	SepV colon = func_to_sepv(builtin_create(object_op_colon, 1, "?property_name"));
	props_accept_prop(Object, sepstr_create(":"), method_create(colon));

	return Object;
}
