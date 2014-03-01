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

#define SEPTEMBER_VERSION "0.1-ambition"

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
		SepFunc *to_string = prop_as_func(to_print, "toString", &err);
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
//  'if..elseif..else'
// ===============================================================

SepObj *proto_IfStatement;

SepItem substatement_elseif(SepObj *scope, ExecutionFrame *frame) {
	SepV ifs = target(scope);
	SepV condition = param(scope, "condition");
	SepV body = param(scope, "body");

	// extract the existing 'branches' array
	SepArray *branches = sepv_to_array(property(ifs, "branches"));

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

SepItem func_if(SepObj *scope, ExecutionFrame *frame) {
	// delegate to statement if
	SepError err = NO_ERROR;
	SepV statement = statement_if(scope, frame).value;
	SepFunc *executor = prop_as_func(statement, "..!", &err);
		or_raise_with_msg(builtin_exception("InternalError"), "Malformed if statement object.");
	return vm_subcall(frame->vm, executor, 0);
}

SepItem statement_if_impl(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;

	SepV ifs = target(scope);
	SepArray *branches = (SepArray*)prop_as_obj(ifs, "branches", &err);
		or_raise(builtin_exception("InternalError"));

	// iterate over all the branches guarded with conditions
	// TODO: replace with better iteration
	int branch_index, branch_count = array_length(branches);
	for (branch_index = 0; branch_index < branch_count; branch_index++) {
		SepV branch = array_get(branches, branch_index);

		// evaluate condition
		SepV condition_l = property(branch, "condition");
		SepV fulfilled = vm_resolve(frame->vm, condition_l);

		if (fulfilled == SEPV_TRUE) {
			// condition true - execute this branch and return
			SepV body_l = property(branch, "body");
			SepV result = vm_resolve(frame->vm, body_l);
			return item_rvalue(result);
		}
	}

	// none of the conditions match - do we have an 'else'?
	SepV else_l = property(ifs, "else_branch");
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
	SepObj *IfStatement = make_class("IfStatement", NULL);

	obj_add_builtin_method(IfStatement, "else..", substatement_else, 1, "body");
	obj_add_builtin_method(IfStatement, "elseif..", substatement_elseif, 2, "?condition", "body");
	obj_add_builtin_method(IfStatement, "..!", statement_if_impl, 0);

	return IfStatement;
}

// ===============================================================
//  While
// ===============================================================

SepItem func_while(SepObj *scope, ExecutionFrame *frame) {
	SepV condition_l = param(scope, "condition");
	SepV condition = vm_resolve(frame->vm, condition_l);
		or_propagate(condition);

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
			or_propagate(result);

		// break?
		if (result == SEPV_BREAK) {
			break;
		}

		// recalculate condition
		condition = vm_resolve(frame->vm, condition_l);
			or_propagate(condition);
	}

	// while() has no return value on normal exit
	return si_nothing();
}

// ===============================================================
//  New try..catch..finally
// ===============================================================

SepObj *proto_TryStatement;

SepItem substatement_catch(SepObj *scope, ExecutionFrame *frame) {
	SepV try_s = target(scope);
	SepV type = param(scope, "type");
	SepV body = param(scope, "body");

	// extract catchers array
	SepArray *catchers = sepv_to_array(property(try_s, "catchers"));

	// push a new catcher
	SepObj *catcher = obj_create_with_proto(SEPV_NOTHING);
	obj_add_field(catcher, "type", type);
	obj_add_field(catcher, "body", body);
	array_push(catchers, obj_to_sepv(catcher));

	// return the statement
	return item_rvalue(try_s);
}

SepItem substatement_finally(SepObj *scope, ExecutionFrame *frame) {
	SepV try_s = target(scope);
	SepV body = param(scope, "body");

	// extract finalizers array
	SepArray *finalizers = sepv_to_array(property(try_s, "finalizers"));

	// push new finalizer
	array_push(finalizers, body);

	// return the statement
	return item_rvalue(try_s);
}

SepItem statement_try(SepObj *scope, ExecutionFrame *frame) {
	SepV body_l = param(scope, "body");

	// build statement object
	SepObj *try_s = obj_create_with_proto(obj_to_sepv(proto_TryStatement));
	obj_add_field(try_s, "body", body_l);
	obj_add_field(try_s, "catchers", obj_to_sepv(array_create(1)));
	obj_add_field(try_s, "finalizers", obj_to_sepv(array_create(1)));

	// return reified statement
	return item_rvalue(obj_to_sepv(try_s));
}

SepItem statement_try_impl(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;

	// execute the body
	SepV try_s = target(scope);
	SepV try_body_l = property(try_s, "body");
	SepV try_result = vm_resolve(frame->vm, try_body_l);

	// was there an exception?
	if (sepv_is_exception(try_result)) {
		// yes, we have to handle it
		// go over the catch clauses and try to catch it
		SepArray *catchers = sepv_to_array(property(try_s, "catchers"));
		int catch_index, catch_count = array_length(catchers);
		for (catch_index = 0; catch_index < catch_count; catch_index++) {
			SepV catcher_obj = array_get(catchers, catch_index);

			// check the exception type using Exception.is().
			SepV catcher_type = property(catcher_obj, "type");
			SepFunc *is_f = prop_as_func(try_result, "is", &err);
				or_raise(builtin_exception("EWrongType"));
			SepV type_matches_v = vm_subcall(frame->vm, is_f, 1, catcher_type).value;
			if (type_matches_v != SEPV_TRUE) {
				// type doesn't match - try another catcher
				continue;
			}

			// the type matches, run the catcher body
			SepV catcher_body = property(catcher_obj, "body");
			SepV catch_result = vm_resolve(frame->vm, catcher_body);

			// the catch body might have thrown exceptions, unfortunately
			if (sepv_is_exception(catch_result)) {
				// we give up in that case
				return item_rvalue(catch_result);
			}

			// phew, exception handled!
			try_result = SEPV_NOTHING;
			break;
		}
	}

	// regardless of whether it was an exception, go through all
	// finalizers
	SepArray *finalizers = sepv_to_array(property(try_s, "finalizers"));
	int fin_index, fin_count = array_length(finalizers);
	for (fin_index = 0; fin_index < fin_count; fin_index++) {
		SepV finalizer_l = array_get(finalizers, fin_index);
		SepV fin_result = vm_resolve(frame->vm, finalizer_l);

		// the finalizer might have thrown an exception, unfortunately
		if (sepv_is_exception(fin_result)) {
			// we give up in that case
			return item_rvalue(fin_result);
		}
	}

	// return the final result (Nothing if there was an exception)
	return item_rvalue(try_result);
}

SepObj *create_try_statement_prototype() {
	SepObj *TryStatement = make_class("TryStatement", NULL);

	obj_add_builtin_method(TryStatement, "catch..", substatement_catch, 2, "type", "body");
	obj_add_builtin_method(TryStatement, "finally..", substatement_finally, 1, "body");
	obj_add_builtin_method(TryStatement, "..!", statement_try_impl, 0);

	return TryStatement;
}

// ===============================================================
//  Old try..catch..finally
// ===============================================================

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
			SepFunc *exception_is = prop_as_func(result, "is", &err);
				or_raise_with_msg(builtin_exception("EWrongType"), "No 'is()' method missing on exception value.");
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

	// flow control
	proto_IfStatement = create_if_statement_prototype();
	obj_add_builtin_func(obj_Syntax, "if", &func_if, 2, "?condition", "body");
	obj_add_builtin_func(obj_Syntax, "if..", &statement_if, 2, "?condition", "body");

	proto_TryStatement = create_try_statement_prototype();
	obj_add_builtin_func(obj_Syntax, "try..", &statement_try, 1, "body");

	obj_add_builtin_func(obj_Syntax, "while", &func_while, 2, "?condition", "body");

	// built-in functions are initialized
	obj_add_builtin_func(obj_Globals, "print", &func_print, 1, "what");
}
