/*****************************************************************
 **
 ** runtime/arrayp.c
 **
 ** Defines the array prototype and all the basic array operations,
 ** including indexing.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "runtime.h"
#include "support.h"
#include "../vm/types.h"
#include "../vm/arrays.h"

// ===============================================================
//  Iteration
// ===============================================================

SepObj *ArrayIterator;

// Creates a new iterator for a given array.
SepItem array_iterator(SepObj *scope, ExecutionFrame *frame) {
	SepArray *this = sepv_to_array(target(scope));

	SepObj *iterator_obj = obj_create_with_proto(obj_to_sepv(ArrayIterator));
	SepArrayIterator *iterator = mem_allocate(sizeof(SepArrayIterator));
	*iterator = array_iterate_over(this);
	iterator_obj->data = iterator;

	return si_obj(iterator_obj);
}

SepItem array_test(SepObj *scope, ExecutionFrame *frame) {
	SepArray *array = array_create(3);
	int i;
	for (i = 1; i <= 3; i++)
		array_push(array, int_to_sepv(i));
	return si_obj(array);
}

// Actual iteration over the array
SepItem arrayiterator_next(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;

	// extract the iterator from the object
	SepObj *target = target_as_obj(scope, &err);
		or_raise(builtin_exception("EWrongType"));
	SepArrayIterator *iterator = target->data;

	// end of iteration?
	if (arrayit_end(iterator))
		return si_exception(builtin_exception("ENoMoreElements"), sepstr_create("No more elements."));

	// nope, return element
	SepV element = arrayit_next(iterator);
	return item_rvalue(element);
}

// ===============================================================
//  Putting the prototype together
// ===============================================================

SepObj *create_array_prototype() {
	// create related prototypes
	ArrayIterator = make_class("ArrayIterator", NULL);
	obj_add_builtin_method(ArrayIterator, "next", arrayiterator_next, 0);

	// create Array prototype
	SepObj *Array = make_class("Array", NULL);
	obj_add_builtin_method(Array, "iterator", array_iterator, 0);
	// TODO: remove this when we have a way to create an array from September code
	obj_add_builtin_method(Array, "test", array_test, 0);

	return Array;
}
