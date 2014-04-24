/*****************************************************************
 **
 ** exceptions.c
 **
 **
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <septvm.h>

// ===============================================================
//  Creating built-in exception types
// ===============================================================

SepObj *obj_add_exception(SepObj *object, char *exception_name, SepObj *parent_type) {
	SepObj *exception_class = make_class(exception_name, parent_type);
	obj_add_field(object, exception_name, obj_to_sepv(exception_class));
	return exception_class;
}

SepObj *create_builtin_exceptions() {
	SepObj *exceptions = obj_create();

	// base exception class for user exceptions
	SepObj *Exception = obj_add_exception(exceptions, "Exception", NULL);

	// internal error type (does not belong to the 'Exception' hierarchy)
	obj_add_exception(exceptions, "EInternal", NULL);

	// built-in exception types
	obj_add_exception(exceptions, "EWrongType", Exception);
	obj_add_exception(exceptions, "EWrongIndex", Exception);
	obj_add_exception(exceptions, "EMissingProperty", Exception);
	obj_add_exception(exceptions, "EPropertyAlreadyExists", Exception);
	obj_add_exception(exceptions, "ENumericOverflow", Exception);
	obj_add_exception(exceptions, "EWrongArguments", Exception);
	obj_add_exception(exceptions, "ECannotAssign", Exception);
	obj_add_exception(exceptions, "ENoMoreElements", Exception);
	obj_add_exception(exceptions, "EBreak", Exception);
	obj_add_exception(exceptions, "EContinue", Exception);

	// return the prototype
	return exceptions;
}
