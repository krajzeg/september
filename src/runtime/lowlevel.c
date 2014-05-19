/*****************************************************************
 **
 ** runtime/lowlevel.c
 **
 ** Implementation of the 'vm' object which allows access to several
 ** low-level functionalities for now. This object will be moved to
 ** its own module once imports are available.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <septvm.h>

// ===============================================================
//  Methods
// ===============================================================

SepItem vm_caller_scope(SepObj *scope, ExecutionFrame *frame) {
	// who is calling us?
	ExecutionFrame *our_caller = frame->prev_frame;
	ExecutionFrame *callers_caller = our_caller->prev_frame;
	if (!callers_caller)
		return si_nothing();

	// return their locals
	return item_rvalue(callers_caller->locals);
}

// ===============================================================
//  Setting up the object
// ===============================================================

SepObj *create_vm_object() {
	SepObj *vm = obj_create();

	obj_add_builtin_method(vm, "callerScope", &vm_caller_scope, 0);

	return vm;
}
