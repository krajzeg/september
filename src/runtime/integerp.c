/*****************************************************************
 **
 ** runtime/lowlevel.c
 **
 ** Implementation of the Integer prototype.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <septvm.h>

// ===============================================================
//  Helpful defines
// ===============================================================

#define INT_MAX ((1LL << 60) - 1LL)
#define INT_MIN (-(1LL << 60))

// ===============================================================
//  Helpers
// ===============================================================

SepV get_params(SepObj *scope, SepInt *i1, SepInt *i2) {
	SepV err = SEPV_NOTHING;
	*i1 = target_as_int(scope, &err);
	*i2 = param_as_int(scope, "other", &err);
	return err;
}

// ===============================================================
//  Arithmetics
// ===============================================================

SepItem overflow_safe_add(SepInt int1, SepInt int2) {
	// check for overflow
	if ((int1 < 0) && (int2 < 0) && (INT_MIN - int1 > int2))
		goto overflow;
	if ((int1 > 0) && (int2 > 0) && (INT_MAX - int1 < int2))
		goto overflow;

	// no overflow, just a new int
	return si_int(int1 + int2);

overflow:
	raise(exc.ENumericOverflow, "'%lld' + '%lld' doesn't fit in 61 bits.");
}

SepItem integer_op_add(SepObj *scope, ExecutionFrame *frame) {
	SepInt a, b;
	SepV err = get_params(scope, &a, &b);
		or_raise(err);

	return overflow_safe_add(a, b);
}

SepItem integer_op_sub(SepObj *scope, ExecutionFrame *frame) {
	SepInt a, b;
	SepV err = get_params(scope, &a, &b);
		or_raise(err);

	return overflow_safe_add(a, -b);
}

SepItem integer_op_mul(SepObj *scope, ExecutionFrame *frame) {
	SepInt a, b;
	SepV err = get_params(scope, &a, &b);
		or_raise(err);

	// TODO: check for overflow
	return si_int(a*b);
}

SepItem integer_op_div(SepObj *scope, ExecutionFrame *frame) {
	SepInt a, b;
	SepV err = get_params(scope, &a, &b);
		or_raise(err);

	return si_int(a/b);
}

SepItem integer_op_mod(SepObj *scope, ExecutionFrame *frame) {
	SepInt a, b;
	SepV err = get_params(scope, &a, &b);
		or_raise(err);

	return si_int(a%b);
}

SepItem integer_op_negate(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepInt integer = target_as_int(scope, &err);
		or_raise(err);


	return overflow_safe_add(0, -integer);
}

// ===============================================================
//  Relations
// ===============================================================

int compare_params(SepObj *scope, SepV *error) {
	SepInt a, b;
	*error = get_params(scope, &a, &b);
	return (a < b) ? -1 : ((a == b) ? 0 : 1);
}

SepItem integer_op_eq(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	int comparison = compare_params(scope, &err); or_raise(err);
	return si_bool(comparison == 0);
}

SepItem integer_op_neq(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	int comparison = compare_params(scope, &err); or_raise(err);
	return si_bool(comparison != 0);
}

SepItem integer_op_lt(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	int comparison = compare_params(scope, &err); or_raise(err);
	return si_bool(comparison < 0);
}

SepItem integer_op_gt(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	int comparison = compare_params(scope, &err); or_raise(err);
	return si_bool(comparison > 0);
}

SepItem integer_op_leq(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	int comparison = compare_params(scope, &err); or_raise(err);
	return si_bool(comparison <= 0);
}

SepItem integer_op_geq(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	int comparison = compare_params(scope, &err); or_raise(err);
	return si_bool(comparison >= 0);
}

// ===============================================================
//  Methods
// ===============================================================

SepItem integer_to_string(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;
	SepInt integer = target_as_int(scope, &err);
		or_raise(err);

	return item_rvalue(str_to_sepv(sepstr_sprintf("%lld", integer)));
}

// ===============================================================
//  Building the prototype
// ===============================================================

SepObj *create_integer_prototype() {
	SepObj *Integer = make_class("Integer", NULL);

	// arithmetics
	obj_add_builtin_method(Integer, "+", integer_op_add, 1, "other");
	obj_add_builtin_method(Integer, "-", integer_op_sub, 1, "other");
	obj_add_builtin_method(Integer, "*", integer_op_mul, 1, "other");
	obj_add_builtin_method(Integer, "/", integer_op_div, 1, "other");
	obj_add_builtin_method(Integer, "%", integer_op_mod, 1, "other");
	obj_add_builtin_method(Integer, "unary-", integer_op_negate, 0);

	// relations
	obj_add_builtin_method(Integer, "==", integer_op_eq,  1, "other");
	obj_add_builtin_method(Integer, "!=", integer_op_neq, 1, "other");
	obj_add_builtin_method(Integer, "<",  integer_op_lt,  1, "other");
	obj_add_builtin_method(Integer, ">",  integer_op_gt,  1, "other");
	obj_add_builtin_method(Integer, "<=", integer_op_leq, 1, "other");
	obj_add_builtin_method(Integer, ">=", integer_op_geq, 1, "other");

	// methods
	obj_add_builtin_method(Integer, "toString", integer_to_string, 0);

	// return prototype
	return Integer;
}
