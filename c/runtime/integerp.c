/*****************************************************************
 **
 ** runtime/integerp.c
 **
 ** Implementation of the Integer prototype.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "runtime.h"
#include "support.h"

// ===============================================================
//  Helpful defines
// ===============================================================

#define INT_MAX ((1LL << 60) - 1LL)
#define INT_MIN (-(1LL << 60))

// ===============================================================
//  Arithmetics
// ===============================================================

SepItem integer_op_add(SepObj *scope, ExecutionFrame *frame) {
	// extract parameters
	SepError err = NO_ERROR;
	SepInt int1 = target_as_int(scope, &err);
		or_raise(NULL);
	SepInt int2 = param_as_int(scope, "other", &err);
		or_raise(NULL);

	// check for overflow
	if ((int1 < 0) && (int2 < 0) && (INT_MIN - int1 > int2))
		goto overflow;
	if ((int1 > 0) && (int2 > 0) && (INT_MAX - int1 < int2))
		goto overflow;

	// no overflow, just a new int
	return si_int(int1 + int2);

overflow:
	raise(NULL, "Numeric overflow: '%lld' + '%lld' doesn't fit in 61 bits.");
}

// ===============================================================
//  Building the prototype
// ===============================================================

SepObj *create_integer_prototype() {
	SepObj *Integer = obj_create();

	// operators
	obj_add_builtin_method(Integer, "+", integer_op_add, 1, "other");

	// return prototype
	return Integer;
}
