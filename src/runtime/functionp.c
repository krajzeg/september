/*****************************************************************
 **
 ** functionp.c
 **
 ** The Function prototype implementation.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <septvm.h>

// ===============================================================
//  Argument source for indirect calls
// ===============================================================



// ===============================================================
//  Indirect calls
// ===============================================================

SepItem function_invoke(SepObj *scope, ExecutionFrame *frame) {
	// what to call?
	SepFunc *callable = sepv_call_target(target(scope));
	if (!callable)
		raise(exc.EWrongType, "Cannot invoke() an object that is not callable.");

	// prepare the argument source
	SepV arguments_v = param(scope, "arguments");
	if (arguments_v == SEPV_NO_VALUE)
		arguments_v = obj_to_sepv(array_create(0));
	if (!sepv_is_array(arguments_v))
		raise(exc.EInternal, "invoke() does not support non-array iterables as arguments yet.");
	SepArray *arguments = sepv_to_array(arguments_v);
	ArrayArgs argument_source;
	arrayargs_init(&argument_source, arguments);

	// check if we have to override the 'this' object
	SepV this_v = param(scope, "target");
	if (this_v != SEPV_NO_VALUE) {
		callable = (SepFunc*)boundmethod_create(callable, this_v);
	}

	// get the custom scope (or SEPV_NO_VALUE if there is none)
	SepV custom_scope_v = param(scope, "scope");

	// invoke!
	SepItem result = vm_invoke_with_argsource(frame->vm,
			func_to_sepv(callable),
			custom_scope_v,
			(ArgumentSource*)&argument_source);

	// return the result, whatever it is
	return result;
}

// ===============================================================
//  Slot prototype
// ===============================================================

SepObj *create_function_prototype() {
	SepObj *Function = make_class("Function", rt.Object);

	obj_add_builtin_method(Function, "invoke", &function_invoke, 3, "=arguments", "=target", "=scope");

	return Function;
}
