/*****************************************************************
 **
 ** runtime/globals.c
 **
 ** The main entry point for initializing the 'globals' runtime
 ** object that stores constants like 'True', classes like 'Integer'
 ** or 'EWrongType', and in general everything that should be
 ** accessible in a September program since its first statement.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================
#include <stdio.h>
#include <stdlib.h>
#include <septvm.h>

// ===============================================================
//  Version
// ===============================================================

#define SEPTEMBER_VERSION "0.1-bailiwick"

// ===============================================================
//  Prototype creation methods
// ===============================================================

SepObj *create_object_prototype();
SepObj *create_class_object();

SepObj *create_array_prototype();
SepObj *create_integer_prototype();
SepObj *create_string_prototype();
SepObj *create_slot_prototype();
SepObj *create_bool_prototype();
SepObj *create_nothing_prototype();
SepObj *create_function_prototype();

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
	SepArray *things = sepv_to_array(param(scope, "what"));

	SepArrayIterator it = array_iterate_over(things);
	bool first = true;
	while (!arrayit_end(&it)) {
		SepV thing = arrayit_next(&it);
		SepString *string;

		if (!sepv_is_str(thing)) {
			// maybe we have a toString() method?
			SepV to_string = property(thing, "toString");
				or_propagate(to_string);
			SepItem string_i = vm_invoke(frame->vm, to_string, 0);
				or_propagate(string_i.value);
			string = cast_as_named_str("Return value of toString()", string_i.value, &err);
				or_raise(exc.EWrongType);
		} else {
			string = sepv_to_str(thing);
		}

		// print it!
		printf(first ? "%s" : " %s", string->cstr);
		first = false;
	}
	puts("");

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

SepItem statement_if_impl(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;

	SepV ifs = target(scope);
	SepArray *branches = (SepArray*) prop_as_obj(ifs, "branches", &err);
	or_raise(exc.EInternal);

	// execution scope for all the functions
	SepV parent_scope = frame->prev_frame->locals;

	// iterate over all the branches guarded with conditions
	SepArrayIterator it = array_iterate_over(branches);
	while (!arrayit_end(&it)) {
		// next branch
		SepV branch = arrayit_next(&it);

		// evaluate condition
		SepV condition_l = property(branch, "condition");
			or_propagate(condition_l);
		SepV fulfilled = vm_resolve(frame->vm, condition_l);
			or_propagate(fulfilled);

		if (fulfilled == SEPV_TRUE) {
			// condition true - execute this branch and return
			SepV body_v = property(branch, "body");
			SepV result = vm_invoke_in_scope(frame->vm, body_v, parent_scope, 0).value;
			return item_rvalue(result);
		}
	}

	// none of the conditions match - do we have an 'else'?
	SepV else_v = property(ifs, "else_branch");
	if (else_v != SEPV_NOTHING) {
		// there was an 'else', so execute that
		SepV result = vm_invoke_in_scope(frame->vm, else_v, parent_scope, 0).value;
		return item_rvalue(result);
	} else {
		// no branch matched, did nothing, return nothing
		return si_nothing();
	}
}

SepItem func_if(SepObj *scope, ExecutionFrame *frame) {
	// standalone if, delegate to substatement if
	SepV statement = statement_if(scope, frame).value;
	obj_add_field(scope, "this", statement);
	return statement_if_impl(scope, frame);
}

SepObj *create_if_statement_prototype() {
	SepObj *IfStatement = make_class("IfStatement", NULL);

	obj_add_builtin_method(IfStatement, "else..", substatement_else, 1, "body");
	obj_add_builtin_method(IfStatement, "elseif..", substatement_elseif, 2,
			"?condition", "body");
	obj_add_builtin_method(IfStatement, "..!", statement_if_impl, 0);

	return IfStatement;
}

// ===============================================================
//  Loop support for all types of loops
// ===============================================================

SepObj *proto_LoopBodyMixin;

SepItem break_impl(SepObj *scope, ExecutionFrame *frame) {
	raise(exc.EBreak, "Uncaught 'break'.");
}

SepItem continue_impl(SepObj *scope, ExecutionFrame *frame) {
	raise(exc.EContinue, "Uncaught 'continue'.");
}

SepObj *create_loop_body_mixin() {
	SepObj* LoopBodyMixin = obj_create();

	SepFunc *break_f = (SepFunc*) builtin_create(&break_impl, 0);
	obj_add_slot(LoopBodyMixin, "break", &st_magic_word, func_to_sepv(break_f));
	SepFunc *continue_f = (SepFunc*) builtin_create(&continue_impl, 0);
	obj_add_slot(LoopBodyMixin, "continue", &st_magic_word,
			func_to_sepv(continue_f));

	return LoopBodyMixin;
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

	// add 'break' and 'continue' support inside the body
	obj_add_prototype(while_body_scope, obj_to_sepv(proto_LoopBodyMixin));

	// loop!
	SepV body_l = param(scope, "body");
	while (condition == SEPV_TRUE) {
		// release condition for GC
		gc_release(condition);
		// execute body
		SepV result = vm_invoke_in_scope(frame->vm, body_l,
				obj_to_sepv(while_body_scope), 0).value;
		// break or continue?
		if (sepv_is_exception(result)) {
			if (has_prototype(result, obj_to_sepv(exc.EBreak))) {
				// break out of the C loop
				break;
			} else if (has_prototype(result, obj_to_sepv(exc.EContinue))) {
				// just ignore the exception
			} else {
				// propagate other exceptions up
				return item_rvalue(result);
			}
		}

		// release result
		gc_release(result);
		// recalculate condition
		condition = vm_resolve(frame->vm, condition_l);
		or_propagate(condition);
	}

	// while() has no return value on normal exit
	return si_nothing();
}

// ===============================================================
//  For..in loop
// ===============================================================

SepObj *proto_ForStatement;

// Creates the for-statement object.
SepItem statement_for(SepObj *scope, ExecutionFrame *frame) {
	SepObj *for_s = obj_create_with_proto(obj_to_sepv(proto_ForStatement));

	SepV variable_name = vm_resolve_as_literal(frame->vm,
			param(scope, "variable_name"));
	obj_add_field(for_s, "variable_name", variable_name);

	return si_obj(for_s);
}

// The "in" part of the for..in statement.
SepItem substatement_in(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;
	SepObj *for_s = target_as_obj(scope, &err);
	or_raise(exc.EWrongType);

	obj_add_field(for_s, "collection", param(scope, "collection"));
	obj_add_field(for_s, "body", param(scope, "body"));

	return si_obj(for_s);
}

// The actual implementation of the for..in loop.
SepItem statement_for_impl(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;

	// get everything out of the statement
	SepV for_s = target(scope);
	SepString *variable_name = prop_as_str(for_s, "variable_name", &err);
	or_raise(exc.EWrongType);
	SepV collection = property(for_s, "collection");
	SepV iterator = call_method(frame->vm, collection, "iterator", 0);
	or_propagate(iterator);
	SepV iterator_next = property(iterator, "next");
	or_propagate(iterator_next);
	SepV body_l = property(for_s, "body");

	// prepare the scope
	SepObj *for_body_scope = obj_create_with_proto(frame->prev_frame->locals);

	// add 'break' and 'continue' support inside the body
	obj_add_prototype(for_body_scope, obj_to_sepv(proto_LoopBodyMixin));

	// add a field for current element
	props_add_prop(for_body_scope, variable_name, &st_field, SEPV_NOTHING);
	SepV for_body_scope_v = obj_to_sepv(for_body_scope);

	// actually start the loop
	while (true) {
		// get the next element in the collection
		SepV element = vm_invoke(frame->vm, iterator_next, 0).value;
		if (sepv_is_exception(element)) {
			// check if its ENoMoreElements - if so, break out of the loop
			SepV enomoreelements_v = obj_to_sepv(exc.ENoMoreElements);
			SepV is_no_more_elements_v = call_method(frame->vm, element, "is",
					1, enomoreelements_v);
			or_propagate(is_no_more_elements_v);
			if (is_no_more_elements_v == SEPV_TRUE) {
				break;
			}

			// not an iteration related exception - just propagate
			return item_rvalue(element);
		}

		// execute the body of the loop
		props_set_prop(for_body_scope, variable_name, element);
		SepV result =
				vm_invoke_in_scope(frame->vm, body_l, for_body_scope_v, 0).value;

		// break or continue?
		if (sepv_is_exception(result)) {
			if (has_prototype(result, obj_to_sepv(exc.EBreak))) {
				// break out of the C loop
				break;
			} else if (has_prototype(result, obj_to_sepv(exc.EContinue))) {
				// just ignore the exception
			} else {
				// propagate other exceptions up
				return item_rvalue(result);
			}
		}

		// release objects from this iteration of the loop to make them GC'able
		gc_release(element);
		gc_release(result);
	}

	// for doesn't return anything normally
	return si_nothing();
}

SepObj *create_for_statement_prototype() {
	SepObj *ForStatement = make_class("ForStatement", NULL);

	obj_add_builtin_method(ForStatement, "in..", substatement_in, 2,
			"collection", "body");
	obj_add_builtin_method(ForStatement, "..!", statement_for_impl, 0);

	return ForStatement;
}

// ===============================================================
//  Try..catch..finally
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
	// execute the body
	SepV try_s = target(scope);
	SepV try_body_v = property(try_s, "body");
	SepV try_result = vm_invoke(frame->vm, try_body_v, 0).value;

	// was there an exception?
	if (sepv_is_exception(try_result)) {
		// yes, we have to handle it
		// go over the catch clauses and try to catch it
		SepArray *catchers = sepv_to_array(property(try_s, "catchers"));
		SepArrayIterator it = array_iterate_over(catchers);
		while (!arrayit_end(&it)) {
			SepV catcher_obj = arrayit_next(&it);

			// check the exception type using Exception.is().
			SepV catcher_type = property(catcher_obj, "type");
			SepV type_matches_v = call_method(frame->vm, try_result, "is", 1,
					catcher_type);
			if (type_matches_v != SEPV_TRUE) {
				// type doesn't match - try another catcher
				continue;
			}

			// the type matches, run the catcher body
			SepV catcher_body = property(catcher_obj, "body");
			SepV catch_result = vm_invoke(frame->vm, catcher_body, 0).value;
			or_propagate(catch_result);

			// phew, exception handled!
			try_result = SEPV_NOTHING;
			break;
		}
	}

	// regardless of whether it was an exception, go through all
	// finalizers
	SepArray *finalizers = sepv_to_array(property(try_s, "finalizers"));
	SepArrayIterator it = array_iterate_over(finalizers);
	while (!arrayit_end(&it)) {
		SepV finalizer_v = arrayit_next(&it);
		SepV finalizer_result = vm_invoke(frame->vm, finalizer_v, 0).value;
		or_propagate(finalizer_result);
	}

	// return the final result (Nothing if there was an exception)
	return item_rvalue(try_result);
}

SepObj *create_try_statement_prototype() {
	SepObj *TryStatement = make_class("TryStatement", NULL);

	obj_add_builtin_method(TryStatement, "catch..", substatement_catch, 2,
			"type", "body");
	obj_add_builtin_method(TryStatement, "finally..", substatement_finally, 1,
			"body");
	obj_add_builtin_method(TryStatement, "..!", statement_try_impl, 0);

	return TryStatement;
}


// ===============================================================
//  Helpers for declarations
// ===============================================================

SepItem func_export(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;

	// find the caller scope
	ExecutionFrame *export_caller = frame->prev_frame;
	ExecutionFrame *export_callers_caller = export_caller->prev_frame;
	if (!export_callers_caller) {
		raise(exc.EInternal, "export() was called from the top-most stack frame (main module's top level) - no place to export to.");
	}
	SepV target_scope_v = export_callers_caller->locals;
	SepObj *target_scope = cast_as_named_obj("Target for export", target_scope_v, &err);
		or_raise(exc.EWrongType);

	// retrieve the object to export and its name
	SepV object = vm_resolve(frame->vm, param(scope, "object"));
	SepV name_v = param(scope, "as");
	if (name_v == SEPV_NO_VALUE) {
		// use the same name as our local name for the object
		name_v = vm_resolve_as_literal(frame->vm, param(scope, "object"));
	}
	SepString *name = cast_as_named_str("Export name", name_v, &err);
		or_raise(exc.EWrongType);

	// export!
	props_add_prop(target_scope, name, &st_field, object);
	return si_nothing();
}

// ===============================================================
//  Runtime initialization
// ===============================================================

SepObj *create_globals() {
	// the class object is initialized first, as all other objects created below are classes
	rt.Cls = create_class_object();

	// Object is a superclass for everything, so it gets created next
	rt.Object = create_object_prototype();
	rt.Cls->prototypes = obj_to_sepv(rt.Object);

	// create the superglobals and establish their relationship
	SepObj *obj_Globals = obj_create();
	SepObj *obj_Syntax = obj_create();
	obj_add_field(obj_Globals, "globals", obj_to_sepv(obj_Globals));
	obj_add_field(obj_Globals, "syntax", obj_to_sepv(obj_Syntax));

	// initialize built-in exception classes
	obj_add_prototype(obj_Globals, obj_to_sepv(create_builtin_exceptions()));

	// ensure that syntax is part of the module
	obj_add_prototype(obj_Globals, obj_to_sepv(obj_Syntax));

	// initialize primitive classes
	obj_add_field(obj_Globals, "Object", obj_to_sepv(rt.Object));
	obj_add_field(obj_Globals, "Class", obj_to_sepv(rt.Cls));
	obj_add_field(obj_Globals, "Array", obj_to_sepv(create_array_prototype()));
	obj_add_field(obj_Globals, "Bool", obj_to_sepv(create_bool_prototype()));
	obj_add_field(obj_Globals, "Slot", obj_to_sepv(create_slot_prototype()));
	obj_add_field(obj_Globals, "Integer", obj_to_sepv(create_integer_prototype()));
	obj_add_field(obj_Globals, "String", obj_to_sepv(create_string_prototype()));
	obj_add_field(obj_Globals, "Function", obj_to_sepv(create_function_prototype()));
	obj_add_field(obj_Globals, "NothingType", obj_to_sepv(create_nothing_prototype()));

	// built-in variables are initialized
	obj_add_field(obj_Globals, "version", sepv_string(SEPTEMBER_VERSION));
	obj_add_field(obj_Syntax, "Nothing", SEPV_NOTHING);
	obj_add_field(obj_Syntax, "True", SEPV_TRUE);
	obj_add_field(obj_Syntax, "False", SEPV_FALSE);
	obj_add_field(obj_Globals, "LiteralScope", SEPV_LITERALS);

	// some built-in objects
	obj_add_builtin_func(obj_Globals, "export", &func_export, 2, "?object", "=as");

	// flow control
	proto_IfStatement = create_if_statement_prototype();
	obj_add_builtin_func(obj_Syntax, "if", &func_if, 2, "?condition", "body");
	obj_add_builtin_func(obj_Syntax, "if..", &statement_if, 2, "?condition",
			"body");
	proto_TryStatement = create_try_statement_prototype();
	obj_add_builtin_func(obj_Syntax, "try..", &statement_try, 1, "body");

	// loops
	proto_LoopBodyMixin = create_loop_body_mixin();
	obj_add_builtin_func(obj_Syntax, "while", &func_while, 2, "?condition",
			"body");

	proto_ForStatement = create_for_statement_prototype();
	obj_add_builtin_func(obj_Syntax, "for..", &statement_for, 1,
			"?variable_name");

	// built-in functions
	obj_add_builtin_func(obj_Globals, "print", &func_print, 1, "...what");

	return obj_Globals;
}

// ===============================================================
//  Module interface
// ===============================================================

void MODULE_EXPORT module_initialize_early(SepModule *module, SepError *out_err) {
	// create the runtime
	SepObj *globals = create_globals();
	// initialize our local copy of libseptvm with the right globals
	initialize_runtime_references(obj_to_sepv(globals));
	// the globals are our root object
	module->root = globals;

	// register things we want to prevent from being GC'd
	module_register_private(module, obj_to_sepv(proto_ForStatement));
	module_register_private(module, obj_to_sepv(proto_IfStatement));
	module_register_private(module, obj_to_sepv(proto_TryStatement));
	module_register_private(module, obj_to_sepv(proto_LoopBodyMixin));
}

void MODULE_EXPORT module_initialize_slave_vm(struct LibSeptVMGlobals *globals,
		SepError *out_err) {
	libseptvm_initialize_slave(globals);
}
