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
#include "../vm/arrays.h"
#include "../vm/vm.h"

#include "support.h"

// ===============================================================
//  Version
// ===============================================================

#define SEPTEMBER_VERSION "0.1-airworthy"

// ===============================================================
//  Prototype objects
// ===============================================================

SepObj *obj_Globals;
SepObj *obj_Syntax;

SepObj *proto_Exceptions;

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

SepObj *create_builtin_exceptions();

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
			raise(builtin_exception("EWrongType"), "Value provided to print() is not a string and has no toString() method.");
		SepFunc *to_string = cast_as_func(to_string_i.value, &err);
			or_raise_with_msg(builtin_exception("EWrongType"), "Value provided to print() is not a string and has no toString() method.");

		// invoke it!
		SepItem string_i = vm_subcall(frame->vm, to_string, 0);
		if (sepv_is_exception(string_i.value))
			return string_i;

		// we have a string now
		string = cast_as_named_str("Return value of toString()", string_i.value, &err);
			or_raise(builtin_exception("EWrongType"));
	} else {
		string = sepv_to_str(to_print);
	}

	// print it!
	printf("%s\n", sepstr_to_cstr(string));

	// return value
	return si_nothing();
}

// ===============================================================
//  New flow control
// ===============================================================

SepObj *proto_IfStatement;

SepItem substatement_elseif(SepObj *scope, ExecutionFrame *frame) {
	SepV ifs = target(scope);
	SepV condition = param(scope, "condition");
	SepV body = param(scope, "body");

	// extract the existing 'branches' array
	SepArray *branches = (SepArray*)sepv_to_obj(sepv_get(ifs, sepstr_create("branches")).value);

	// create the 'else-if' branch
	SepObj *branch = obj_create_with_proto(SEPV_NOTHING);
	obj_add_field(branch, "condition", condition);
	obj_add_field(branch, "body", body);
	array_push(branches, obj_to_sepv(branch));

	// done, return the object for further chaining
	return item_rvalue(ifs);
}

SepItem substatement_else(SepObj *scope, ExecutionFrame *frame) {
	SepV ifs = target(scope);
	SepV body = param(scope, "body");

	obj_add_field(sepv_to_obj(ifs), "else_branch", body);

	return item_rvalue(ifs);
}

SepItem statement_if(SepObj *scope, ExecutionFrame *frame) {
	SepV condition = param(scope, "condition");
	SepV body = param(scope, "body");

	SepArray *branches = array_create(1);

	// create the 'true' branch
	SepObj *branch = obj_create_with_proto(SEPV_NOTHING);
	obj_add_field(branch, "condition", condition);
	obj_add_field(branch, "body", body);
	array_push(branches, obj_to_sepv(branch));

	// create if statement object
	SepObj *ifs = obj_create_with_proto(obj_to_sepv(proto_IfStatement));
	obj_add_field(ifs, "branches", obj_to_sepv(branches));
	obj_add_field(ifs, "else_branch", SEPV_NOTHING);

	// return the reified If
	return item_rvalue(obj_to_sepv(ifs));
}

SepItem statement_if_impl(SepObj *scope, ExecutionFrame *frame) {
	SepV ifs = target(scope);
	SepArray *branches = (SepArray*)sepv_to_obj(sepv_get(ifs, sepstr_create("branches")).value);

	// iterate over all the branches guarded with conditions
	// TODO: replace with better iteration
	int branch_index = 0, branch_count = array_length(branches);
	for (; branch_index < branch_count; branch_index++) {
		SepV branch = array_get(branches, branch_index);

		// evaluate condition
		SepV condition_l = sepv_get(branch, sepstr_create("condition")).value;
		SepV fulfilled = vm_resolve(frame->vm, condition_l);

		if (fulfilled == SEPV_TRUE) {
			// condition true - execute this branch and return
			SepV body_l = sepv_get(branch, sepstr_create("body")).value;
			SepV result = vm_resolve(frame->vm, body_l);
			return item_rvalue(result);
		}
	}

	// none of the conditions match - do we have an 'else'?
	SepV else_l = sepv_get(ifs, sepstr_create("else_branch")).value;
	if (else_l != SEPV_NOTHING) {
		// there was an 'else', so execute that
		SepV result = vm_resolve(frame->vm, else_l);
		return item_rvalue(result);
	} else {
		// no branch matched, did nothing, return nothing
		return si_nothing();
	}
}

SepObj *create_if_statement_prototype() {
	SepObj *IfStatement = obj_create();

	obj_add_builtin_method(IfStatement, "else_", substatement_else, 1, "body");
	obj_add_builtin_method(IfStatement, "elseif_", substatement_elseif, 2, "?condition", "body");
	obj_add_builtin_method(IfStatement, "execute_", statement_if_impl, 0);

	return IfStatement;
}

// ===============================================================
//  Flow control
// ===============================================================

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

	// create execution scope for the body
	SepObj *while_body_scope = obj_create_with_proto(frame->prev_frame->locals);
	obj_add_escape(while_body_scope, "break", frame, SEPV_BREAK);
	obj_add_escape(while_body_scope, "continue", frame, SEPV_NOTHING);

	// loop!
	SepV body_l = param(scope, "body");
	while (condition == SEPV_TRUE) {
		// execute body
		SepV result = vm_resolve_in(frame->vm, body_l, obj_to_sepv(while_body_scope));

		if (sepv_is_exception(result)) {
			// propagate exceptions from body
			return item_rvalue(result);
		} else if (result == SEPV_BREAK) {
			break;
		}

		// recalculate condition
		condition = vm_resolve(frame->vm, condition_l);
	}

	// while() has no return value on normal exit
	return si_nothing();
}

SepItem func_try_catch_finally(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;

	SepV try_l = param(scope, "body");
	SepV catch_type_v = param(scope, "handler_type");
	SepV catch_l = param(scope, "handler");
	SepV finally_l = param(scope, "finalizer");

	// try executing the body
	SepV result = vm_resolve(frame->vm, try_l);
	if (sepv_is_exception(result)) {
		// exception - first check the catch clause
		if (catch_l != SEPV_NOTHING) {
			// check for type match
			SepFunc *exception_is = cast_as_named_func("is() method", sepv_get(result, sepstr_create("is")).value, &err);
				or_raise(builtin_exception("EWrongType"));
			bool type_matches = vm_subcall(frame->vm, exception_is, 1, catch_type_v).value == SEPV_TRUE;

			if (type_matches) {
				// handle it!
				SepV catch_result = vm_resolve(frame->vm, catch_l);
					or_propagate(catch_result);

				// caught - clear exception
				result = SEPV_NOTHING;
			}
		}

		// .. and then execute the finalizer
		if (finally_l != SEPV_NOTHING) {
			SepV finally_result = vm_resolve(frame->vm, finally_l);
				or_propagate(finally_result);
		}
	}

	// return final result (whether an exception, or the result from the body)
	return item_rvalue(result);
}

// ===============================================================
//  Runtime initialization
// ===============================================================

void initialize_runtime() {
	// "Object" has to be initialized first, as its the prototype to all other prototypes
	proto_Object = create_object_prototype();

	// create holder objects and establish their relationship
	obj_Globals = obj_create();
	obj_Syntax  = obj_create();
	obj_add_field(obj_Globals, "Globals", obj_to_sepv(obj_Globals));
	obj_add_field(obj_Globals, "Syntax", obj_to_sepv(obj_Syntax));

	// initialize built-in exceptions
	proto_Exceptions = create_builtin_exceptions();
	obj_add_prototype(obj_Globals, obj_to_sepv(proto_Exceptions));

	// primitive types' prototypes are initialized here
	proto_Nothing = create_nothing_prototype();
	obj_add_field(obj_Globals, "Object", obj_to_sepv(proto_Object));
	obj_add_field(obj_Globals, "Bool", obj_to_sepv((proto_Bool = create_bool_prototype())));
	obj_add_field(obj_Globals, "Integer", obj_to_sepv((proto_Integer = create_integer_prototype())));
	obj_add_field(obj_Globals, "String", obj_to_sepv((proto_String = create_string_prototype())));

	// built-in variables are initialized
	obj_add_field(obj_Globals, "version", sepv_string(SEPTEMBER_VERSION));
	obj_add_field(obj_Syntax, "Nothing", SEPV_NOTHING);
	obj_add_field(obj_Syntax, "True", SEPV_TRUE);
	obj_add_field(obj_Syntax, "False", SEPV_FALSE);

	// new flow control
	proto_IfStatement = create_if_statement_prototype();
	obj_add_builtin_func(obj_Syntax, "if_", &statement_if, 2, "?condition", "body");

	// flow control
	obj_add_builtin_func(obj_Syntax, "if", &func_if, 2, "condition", "body");
	obj_add_builtin_func(obj_Syntax, "if..else", &func_if_else, 3, "condition", "true_branch", "false_branch");
	obj_add_builtin_func(obj_Syntax, "while", &func_while, 2, "?condition", "body");

	obj_add_builtin_func(obj_Syntax, "try..catch..finally", &func_try_catch_finally, 4,
			"body", "handler_type", "handler", "finalizer");

	// built-in functions are initialized
	obj_add_builtin_func(obj_Globals, "print", &func_print, 1, "what");
}
