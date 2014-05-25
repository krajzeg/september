/*****************************************************************
 **
 ** specialp.c
 **
 ** File containing prototypes for special values - Nothing and
 ** the booleans.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "common.h"

// ===============================================================
//  Booleans - methods
// ===============================================================

SepItem bool_to_string(SepObj *scope, ExecutionFrame *frame) {
	SepV this = target(scope);
	bool truth = (this == SEPV_TRUE);
	return si_string(truth ? "<True>" : "<False>");
}

// ===============================================================
//  Booleans - operators
// ===============================================================

SepItem bool_not(SepObj *scope, ExecutionFrame *frame) {
	bool truth = target(scope) == SEPV_TRUE;
	return si_bool(!truth);
}

SepItem bool_and(SepObj *scope, ExecutionFrame *frame) {
	bool a = target(scope) == SEPV_TRUE;

	// short-circuiting
	if (!a) return si_bool(false);
	// left-side was true, resolve the right-hand side then
	return item_rvalue(vm_resolve(frame->vm, param(scope, "other")));
}

SepItem bool_or(SepObj *scope, ExecutionFrame *frame) {
	bool a = target(scope) == SEPV_TRUE;

	// short-circuiting
	if (a) return si_bool(true);
	// left-side was false, resolve the right-hand side then
	return item_rvalue(vm_resolve(frame->vm, param(scope, "other")));
}

// ===============================================================
//  Nothing - methods
// ===============================================================

SepItem nothing_to_string(SepObj *scope, ExecutionFrame *frame) {
	return si_string("<Nothing>");
}

// ===============================================================
//  Prototype creation
// ===============================================================

SepObj *create_nothing_prototype() {
	SepObj *NothingP = make_class("Nothing", NULL);

	obj_add_builtin_method(NothingP, "toString", &nothing_to_string, 0);

	return NothingP;
}

SepObj *create_bool_prototype() {
	SepObj *Bool = make_class("Bool", NULL);

	obj_add_builtin_method(Bool, "toString", &bool_to_string, 0);

	obj_add_builtin_method(Bool, "unary!", &bool_not, 0);
	obj_add_builtin_method(Bool, "&&", &bool_and, 1, "?other");
	obj_add_builtin_method(Bool, "||", &bool_or, 1, "?other");

	return Bool;
}
