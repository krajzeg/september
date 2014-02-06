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

#include "runtime.h"
#include "support.h"

// ===============================================================
//  Booleans - methods
// ===============================================================

SepItem bool_to_string(SepObj *scope, ExecutionFrame *frame) {
	SepV this = target(scope);
	bool truth = (this == SEPV_TRUE);
	return si_string(truth ? "<True>" : "<False>");
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
	SepObj *NothingP = obj_create_with_proto(SEPV_NOTHING);

	obj_add_builtin_method(NothingP, "toString", &nothing_to_string, 0);

	return NothingP;
}

SepObj *create_bool_prototype() {
	SepObj *Bool = obj_create_with_proto(SEPV_NOTHING);

	obj_add_builtin_method(Bool, "toString", &bool_to_string, 0);

	return Bool;
}
