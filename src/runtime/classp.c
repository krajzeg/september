/*****************************************************************
 **
 ** classp.c
 **
 ** Defines the magic Class object (which is not used as a prototype,
 ** but more as a factory) and all its properties.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <septvm.h>

// ===============================================================
//  Class methods
// ===============================================================

SepItem class_new(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;
	SepString *class_name = param_as_str(scope, "name", &err);
		or_raise(exc.EWrongType);

	// make class object using libseptvm functionality
	SepObj *cls = make_class(class_name->cstr, rt.Object);

	// return the class
	return si_obj(cls);
}

SepItem class_call(SepObj *scope, ExecutionFrame *frame) {
	SepV target_v = target(scope);
	SepV instance = call_method(frame->vm, target_v, "spawn", 0);
		or_propagate(instance);

	// do we have a constructor?
	bool constructor_present = property_exists(instance, "<constructor>");
	if (constructor_present) {
		// find the constructor
		SepV constructor_v = property(instance, "<constructor>");

		// prepare its arguments
		ArrayArgs constructor_args;
		SepArray *argument_array = sepv_to_array(param(scope, "arguments"));
		arrayargs_init(&constructor_args, argument_array);
		ArgumentSource *arg_source = (ArgumentSource*)&constructor_args;

		// call!
		SepV ctor_result = vm_invoke_with_argsource(frame->vm, constructor_v, SEPV_NO_VALUE, arg_source).value;
			or_propagate(ctor_result);
	}

	return item_rvalue(instance);
}

// ===============================================================
//  Constructing the Class object
// ===============================================================

SepObj *create_class_object() {
	SepObj *Cls = obj_create_with_proto(SEPV_NOTHING);

	// Class object methods
	obj_add_builtin_method(Cls, "new", class_new, 1, "name");

	// functions to copy into instantiated classes
	obj_add_builtin_func(Cls, "<call>", class_call, 1, "...?arguments");

	return Cls;
}
