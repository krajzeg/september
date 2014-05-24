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

#include <septvm.h>

// ===============================================================
//  Iteration
// ===============================================================

// Creates a new iterator for a given array.
SepItem array_iterator(SepObj *scope, ExecutionFrame *frame) {
	SepArray *this = sepv_to_array(target(scope));

	SepV iterator_proto_v = property(obj_to_sepv(this), "<ArrayIterator>");
	SepObj *iterator_obj = obj_create_with_proto(iterator_proto_v);

	// add a reference to the array to prevent it from being GC'd while we iterate
	obj_add_field(iterator_obj, "<array>", obj_to_sepv(this));

	// create the C iterator and store as auxillary data
	iterator_obj->data = mem_allocate(sizeof(SepArrayIterator));
	*((SepArrayIterator*)iterator_obj->data) = array_iterate_over(this);

	// return the iterator object
	return si_obj(iterator_obj);
}

// Actual iteration over the array
SepItem arrayiterator_next(SepObj *scope, ExecutionFrame *frame) {
	SepV err = SEPV_NOTHING;

	// extract the iterator from the object
	SepObj *target = target_as_obj(scope, &err);
		or_raise(exc.EWrongType);
	SepArrayIterator *iterator = target->data;

	// end of iteration?
	if (arrayit_end(iterator))
		return si_exception(exc.ENoMoreElements, sepstr_for("No more elements."));

	// nope, return element
	SepV element = arrayit_next(iterator);
	return item_rvalue(element);
}

// ===============================================================
//  Array index slots
// ===============================================================

/**
 * This type of slot represents one element of the array. The special slot
 * has to be used to allow writing to array indices.
 */
typedef struct ArrayIndexSlot {
	// the standard stuff every slot has
	Slot base;
	// the array from which this value came from
	SepArray *array;
	// the index from which it came
	uint32_t index;
} ArrayIndexSlot;

// Retrieve a value from an array index.
SepV _array_index_slot_retrieve(Slot *slot, OriginInfo *origin) {
	ArrayIndexSlot *this = (ArrayIndexSlot*)slot;
	return array_get(this->array, this->index);
}

// Set a value inside an array element.
SepV _array_index_slot_store(Slot *slot, OriginInfo *origin, SepV new_value) {
	ArrayIndexSlot *this = (ArrayIndexSlot*)slot;
	return array_set(this->array, this->index, new_value);
}

// Queue dependent objects during GC.
void _array_index_slot_mark_and_queue(Slot *slot, GarbageCollection *gc) {
	ArrayIndexSlot *this = (ArrayIndexSlot*)slot;
	gc_add_to_queue(gc, obj_to_sepv(this->array));
}

// The v-table for the array index slot.
SlotType array_index_slot_vt = {
	SF_NOTHING_SPECIAL,
	&_array_index_slot_retrieve,
	&_array_index_slot_store,
	&_array_index_slot_mark_and_queue
};

// Creates a new "array index" slot capable of writing values to the array.
ArrayIndexSlot *array_index_slot_create(SepArray *array, uint32_t index) {
	ArrayIndexSlot *slot = mem_allocate(sizeof(ArrayIndexSlot));
	slot->base.vt = &array_index_slot_vt;
	slot->array = array;
	slot->index = index;
	return slot;
}

// ===============================================================
//  Indexing/slicing
// ===============================================================

SepItem array_index(SepObj *scope, ExecutionFrame *frame) {
	SepArray *this = sepv_to_array(target(scope));
	SepV index_v = param(scope, "index");
	if (!sepv_is_int(index_v))
		raise(exc.EWrongType, "Only integer indices are supported at this point.");

	// determine the actual index
	SepInt index_i = sepv_to_int(index_v);
	uint32_t index;
	if (index_i < 0) {
		index = array_length(this) + index_i;
	} else {
		index = index_i;
	}

	// create the slot used for assignment to array indices
	SepV value = array_get(this, index);
		or_propagate(value);
	Slot *slot = (Slot*)array_index_slot_create(this, index);

	// return an l-value so assigning to indices works
	return item_artificial_lvalue(slot, value);
}

// ===============================================================
//  Putting the prototype together
// ===============================================================

SepObj *create_array_prototype() {
	// create related prototypes
	SepObj *ArrayIterator = make_class("ArrayIterator", NULL);
	obj_add_builtin_method(ArrayIterator, "next", arrayiterator_next, 0);

	// create Array prototype
	SepObj *Array = make_class("Array", NULL);
	obj_add_field(Array, "<ArrayIterator>", obj_to_sepv(ArrayIterator));
	obj_add_builtin_method(Array, "iterator", array_iterator, 0);
	obj_add_builtin_method(Array, "[]", array_index, 1, "index");

	return Array;
}
