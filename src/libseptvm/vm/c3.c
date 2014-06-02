/*****************************************************************
 **
 ** vm/c3.c
 **
 ** Implementation of the C3 property resolution order used in
 ** September objects.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>
#include "../libmain.h"
#include "objects.h"
#include "arrays.h"
#include "support.h"
#include "c3.h"

// ===============================================================
//  Caching
// ===============================================================

// Returns the cached C3 order stored within an object, if there is one.
// If not, returns NULL.
SepArray *c3_cached_order(SepV object_v) {
	if (!sepv_is_obj(object_v))
		return NULL;
	SepObj *object = sepv_to_obj(object_v);
	Slot *cache_slot = props_find_prop(object, sepstr_for("<c3>"));
	if (cache_slot && cache_slot->value != SEPV_NO_VALUE) {
		return sepv_to_array(cache_slot->value);
	} else {
		return NULL;
	}
}

// Returns the version number of the cached C3 order stored within an
// object, or 0 if there isn't one. These version numbers can be used
// to detect when the cache has to be invalidated due to prototype
// changes.
int64_t c3_cache_version(SepV object_v) {
	if (!sepv_is_obj(object_v))
		return 0;
	SepObj *object = sepv_to_obj(object_v);
	Slot *cache_v_slot = props_find_prop(object, sepstr_for("<c3version>"));
	if (cache_v_slot) {
		return sepv_to_int(cache_v_slot->value);
	} else {
		return 0;
	}
}

// Stores the previously calculated C3 order in the object it belongs to,
// along with a version number used to validate this cache.
void c3_store_cached_order(SepV object_v, SepArray *order) {
	if (!sepv_is_obj(object_v))
		return;

	SepObj *object = sepv_to_obj(object_v);
	obj_add_field(object, "<c3>", obj_to_sepv(order));
	obj_add_field(object, "<c3version>", int_to_sepv(lsvm_globals.property_cache_version));
}

// Invalidates the internally cached C3 order stored within the object,
// causing it to be recalculated on next property access.
void c3_invalidate_cache(SepV object_v) {
	if (!sepv_is_obj(object_v))
		return;
	SepObj *object = sepv_to_obj(object_v);
	Slot *cache_slot = props_find_prop(object, sepstr_for("<c3>"));
	if (cache_slot) {
		cache_slot->value = SEPV_NO_VALUE;
        props_set_prop(object, sepstr_for("<c3version>"), ++lsvm_globals.property_cache_version);
	}
}

// ===============================================================
//  C3 calculation
// ===============================================================

// Uses the C3 merge operation to create a resolution order based on
// the order of the prototypes, and their C3 orders.
SepArray *c3_merge(SepArray *orders, SepV *error) {
	SepArray *merged = array_create(1);
	int i = 0, j, count = array_length(orders);
	for (; i < count; i++) {
		SepArray *seq = sepv_to_array(array_get(orders, i));
		if (array_length(seq) == 0)
			continue;
		SepV head = array_get(seq, 0);

		// is this a good head to remove?
		bool good_head = true;
		for (j = 0; j < count; j++) {
			if (i != j) {
				SepArray *other_seq = sepv_to_array(array_get(orders, j));
				int32_t index = array_index_of(other_seq, head);
				if (index >= 1) {
					// nope
					good_head = false;
					break;
				}
			}
		}

		if (good_head) {
			// yes, add it to the merged order
			array_push(merged, head);
			// remove it from all arrays
			for (j = 0; j < count; j++) {
				SepArray *remove_seq = sepv_to_array(array_get(orders, j));
				array_remove(remove_seq, head);
			}
			// ...and start over looking for the next head
			i = -1;
		}
	}

	// done - all lists should be empty now
	for (i = 0; i < count; i++) {
		SepArray *seq = sepv_to_array(array_get(orders, i));
		if (array_length(seq) > 0) {

			fail(NULL, exception(exc.EInternal, "Ambiguous inheritance hierarchy."));
		}
	}

	// they are - OK!
	return merged;
}

// Recalculates the C3 property resolution order for an object based
// on its current state and prototypes.
SepArray *c3_determine_order(SepV object_v, SepV *error) {
	SepV err = SEPV_NO_VALUE;

	// always start with yourself
	SepArray *order = array_create(1);
	array_push(order, object_v);

	// any prototypes?
	SepV prototype_v = sepv_prototypes(object_v);
	if (prototype_v == SEPV_NOTHING) {
		// nope, just me
		return order;
	} else if (!sepv_is_array(prototype_v)) {
		// single prototype, just append its C3
		array_push_all(order, c3_order(prototype_v, &err));
			or_fail_with(NULL);
		// and return
		return order;
	} else {
		// multiple prototypes
		SepArray *prototypes = sepv_to_array(prototype_v);
		SepArray *orders_to_merge = array_create(array_length(prototypes) + 1);
		array_push(orders_to_merge, obj_to_sepv(array_copy(prototypes)));
		SepArrayIterator it = array_iterate_over(prototypes);
		while (!arrayit_end(&it)) {
			SepV prototype = arrayit_next(&it);
			SepArray *prototype_c3 = c3_order(prototype, &err);
				or_fail_with(NULL);
			array_push(orders_to_merge, obj_to_sepv(array_copy(prototype_c3)));
				or_fail_with(NULL);
		}

		// merge using C3 algorithm
		SepArray *merged = c3_merge(orders_to_merge, &err);
			or_fail_with(NULL);
		array_push_all(order, merged);
		return order;
	}
}

// ===============================================================
//  Main interface
// ===============================================================

// Returns the C3 resolution order for the given object. The returned
// array will include all direct and indirect prototypes, sorted according
// to the C3 rules. If it's impossible to resolve an unambiguous order,
// an error will be raised.
SepArray *c3_order(SepV object_v, SepV *error) {
	SepV err = SEPV_NO_VALUE;

	// do we have a resolution order cached?
	if (sepv_is_obj(object_v)) {
		SepObj *object = sepv_to_obj(object_v);
		Slot *cache_slot = props_find_prop(object, sepstr_for("<c3>"));
		if (cache_slot && (cache_slot->value != SEPV_NO_VALUE)) {
			// yes, there is a cached order - return it
			return sepv_to_array(cache_slot->value);
		}
	}

	// no - calculate it
	SepArray *order = c3_determine_order(object_v, &err);
		or_fail_with(NULL);

	// cache for future reference and return
	c3_store_cached_order(object_v, order);
	return order;
}
