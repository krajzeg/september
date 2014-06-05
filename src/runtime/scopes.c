/*****************************************************************
 **
 ** scopes.c
 **
 ** Implements the Scopes object and its static methods.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "common.h"

// ===============================================================
//  Access to different kinds of scopes
// ===============================================================

SepItem scopes_caller(SepObj *scope, ExecutionFrame *frame) {
	// find the caller scope
	ExecutionFrame *our_caller = frame->prev_frame;
	ExecutionFrame *their_caller = our_caller->prev_frame;
	if (!their_caller) {
		raise(exc.EInternal,
				"export() was called from the top-most stack frame (main module's top level) - no place to export to.");
	}
	SepV their_callers_scope = their_caller->locals;
	return item_rvalue(their_callers_scope);
}

// ===============================================================
//  Creating the scopes object
// ===============================================================

SepObj *create_scopes_object() {
	SepObj *Scopes = obj_create();

	obj_add_builtin_func(Scopes, "caller", &scopes_caller, 0);

	return Scopes;
}
