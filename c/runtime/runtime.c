// ===============================================================
//  Includes
// ===============================================================

#include <stdio.h>
#include <stdlib.h>
#include "../common/errors.h"
#include "../vm/functions.h"
#include "../vm/types.h"
#include "../vm/objects.h"
#include "../vm/exceptions.h"
#include "../vm/vm.h"

#include "support.h"

// ===============================================================
//  Prototype objects
// ===============================================================

SepObj *proto_Object;
SepObj *proto_String;
SepObj *proto_Integer;
SepObj *proto_Bool;
SepObj *proto_Nothing;

// ===============================================================
//  Prototype creation methods
// ===============================================================

SepObj *create_object_prototype();
SepObj *create_integer_prototype();
SepObj *create_string_prototype();
SepObj *create_bool_prototype();
SepObj *create_nothing_prototype();

// ===============================================================
//  Built-in functions
// ===============================================================

// hack for MinGW's strange printf situation
#ifdef __MINGW32__
#define printf __mingw_printf
#endif

SepItem print_impl(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;
	SepV to_print = param(scope, "what");
	SepString *string;

	if (!sepv_is_str(to_print)) {
		// maybe we have a toString() method?
		SepItem to_string_i = sepv_get(to_print, sepstr_create("toString"));
		if (sepv_is_exception(to_string_i.value))
			raise(NULL, "Value provided to print() is not a string and has no toString() method.");
		SepFunc *to_string = cast_as_func(to_string_i.value, &err);
			or_raise_with_msg(NULL, "Value provided to print() is not a string and has no toString() method.");

		// invoke it!
		SepItem string_i = vm_subcall(frame->vm, to_string, 0);
		if (sepv_is_exception(string_i.value))
			return string_i;

		// we have a string now
		string = cast_as_named_str("Return value of toString()", string_i.value, &err);
			or_raise(NULL);
	} else {
		string = sepv_to_str(to_print);
	}

	// print it!
	printf("%s\n", sepstr_to_cstr(string));

	// return value
	return si_nothing();
}

void initialize_runtime(SepObj *scope) {
	// "Object" has to be initialized first, as its the prototype to all other prototypes
	proto_Object = create_object_prototype();

	// primitive types' prototypes are initialized here
	proto_Nothing = create_nothing_prototype();
	proto_Bool = create_bool_prototype();
	proto_Integer = create_integer_prototype();
	proto_String = create_string_prototype();

	// built-in variables are initialized
	obj_add_field(scope, "version", sepv_string("0.1-aeon"));
	obj_add_field(scope, "Nothing", SEPV_NOTHING);
	obj_add_field(scope, "True", SEPV_TRUE);
	obj_add_field(scope, "False", SEPV_FALSE);

	// built-in functions are initialized
	obj_add_builtin_func(scope, "print", &print_impl, 1, "what");
}
