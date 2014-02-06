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
//  Version
// ===============================================================

#define SEPTEMBER_VERSION "0.1-agility"

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

SepItem func_print(SepObj *scope, ExecutionFrame *frame) {
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

SepItem func_if(SepObj *scope, ExecutionFrame *frame) {
	SepV condition = param(scope, "condition");
	if (condition == SEPV_TRUE) {
		SepV body = param(scope, "body");
		SepV return_value = vm_resolve(frame->vm, body);
		return item_rvalue(return_value);
	} else {
		return si_nothing();
	}
}

SepItem func_if_else(SepObj *scope, ExecutionFrame *frame) {
	SepV condition = param(scope, "condition");
	SepV body = param(scope, (condition == SEPV_TRUE) ? "true_branch" : "false_branch");
	SepV return_value = vm_resolve(frame->vm, body);
	return item_rvalue(return_value);
}

SepItem func_while(SepObj *scope, ExecutionFrame *frame) {
	SepV condition_l = param(scope, "condition");
	SepV condition = vm_resolve(frame->vm, condition_l);

	// not looping even once?
	if (condition != SEPV_TRUE)
		return si_nothing();

	SepV body_l = param(scope, "body");
	while (condition == SEPV_TRUE) {
		// execute body
		SepV result = vm_resolve(frame->vm, body_l);
		if (sepv_is_exception(result)) {
			// propagate exceptions from body
			return item_rvalue(result);
		}

		// recalculate condition
		condition = vm_resolve(frame->vm, condition_l);
	}

	// while() has no return value
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
	obj_add_field(scope, "version", sepv_string(SEPTEMBER_VERSION));
	obj_add_field(scope, "Nothing", SEPV_NOTHING);
	obj_add_field(scope, "True", SEPV_TRUE);
	obj_add_field(scope, "False", SEPV_FALSE);

	// flow control
	obj_add_builtin_func(scope, "if", &func_if, 2, "condition", "body");
	obj_add_builtin_func(scope, "if..else", &func_if_else, 3, "condition", "true_branch", "false_branch");
	obj_add_builtin_func(scope, "while", &func_while, 2, "?condition", "body");

	// built-in functions are initialized
	obj_add_builtin_func(scope, "print", &func_print, 1, "what");
}
