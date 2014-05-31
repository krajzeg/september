/*****************************************************************
 **
 ** runtime/objects.c
 **
 **	Implementation of objects.h - everything to do with runtime
 **	objects.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "mem.h"
#include "exceptions.h"
#include "functions.h"
#include "objects.h"
#include "arrays.h"
#include "gc.h"
#include "../vm/runtime.h"
#include "../vm/support.h"

// ===============================================================
//  Constants
// ===============================================================

#define PROPERTY_MAP_GROWTH_FACTOR 1.5f

// ===============================================================
//  Slots
// ===============================================================

void slot_init(Slot *slot, SlotType *behavior, SepV initial_value) {
	slot->vt = behavior;
	slot->value = initial_value;
}

Slot *slot_create(SlotType *behavior, SepV initial_value) {
	Slot *slot = mem_allocate(sizeof(Slot));
	slot->vt = behavior;
	slot->value = initial_value;
	gc_register(slot_to_sepv(slot));
	return slot;
}

// Retrieves a value from any type of slot.
SepV slot_retrieve(Slot *slot, OriginInfo *origin) {
	return slot->vt->retrieve(slot, origin);
}

// Stores a value in any type of slot.
SepV slot_store(Slot *slot, OriginInfo *origin, SepV new_value) {
	return slot->vt->store(slot, origin, new_value);
}

// ===============================================================
//  Fields
// ===============================================================

SepV field_retrieve(Slot *slot, OriginInfo *origin) {
	return slot->value;
}

SepV field_store(Slot *slot, OriginInfo *origin, SepV value) {
	return slot->value = value;
}

SlotType st_field = {SF_NOTHING_SPECIAL, &field_retrieve, &field_store, NULL };

// ===============================================================
//  Methods
// ===============================================================

SepV method_retrieve(Slot *slot, OriginInfo *origin) {
	SepV method_v = slot->value;

	// if the value inside is not a function, do nothing special
	if (!sepv_is_func(method_v))
		return method_v;

	// if it is a function, bind it permanently to the host
	// so the 'this' pointer is correct when it is invoked
	SepFunc *func = sepv_to_func(method_v);
	SepFunc *bound = (SepFunc*) boundmethod_create(func, origin->source);
	return func_to_sepv(bound);
}

SepV method_store(Slot *slot, OriginInfo *origin, SepV value) {
	return slot->value = value;
}

SlotType st_method = {SF_NOTHING_SPECIAL, &method_retrieve, &method_store, NULL };

// ===============================================================
//  Magic words
// ===============================================================

// magic words are behave identically to fields - but they have
// the SF_MAGIC_WORD flag turned on to make the VM handle them
// in a special fashion
SlotType st_magic_word = {SF_MAGIC_WORD, &field_retrieve, &field_store, NULL };

// ===============================================================
//  Slots for special properties
// ===============================================================

SepV prototypes_retrieve(Slot *slot, OriginInfo *origin) {
	SepV protos = sepv_prototypes(origin->source);
	if (sepv_is_array(protos)) {
		// return a safe to modify copy
		return obj_to_sepv(array_copy(sepv_to_array(protos)));
	} else {
		// single value, safe to return
		return protos;
	}
}

SepV prototypes_store(Slot *slot, OriginInfo *origin, SepV value) {
	SepV target = origin->source;
	if (!sepv_is_obj(target)) {
		raise_sepv(exc.EInternal, "Changing the prototypes of this object is impossible.");
	}
	SepObj *obj = sepv_to_obj(target);
	obj_set_prototypes(obj, value);
	return value;
}

SlotType st_prototype_list = {SF_NOTHING_SPECIAL, &prototypes_retrieve, &prototypes_store, NULL };

// ===============================================================
//  Property maps - private implementation
// ===============================================================

void _props_resize(PropertyMap *this, int new_capacity);

PropertyEntry *_props_find_entry(PropertyMap *this, SepString *name,
		PropertyEntry **previous) {
	// find the correct first entry for the bucket
	uint32_t index = sepstr_hash(name) % this->capacity;
	PropertyEntry *entry = &this->entries[index];

	// is this the right bucket (empty or containing the named prop?)
	if ((!entry->name) || (!sepstr_cmp(entry->name, name))) {
		*previous = NULL;
		return entry;
	}

	// look through the linked list
	while (entry->next_entry) {
		*previous = entry;
		entry = &this->entries[entry->next_entry];
		if (!sepstr_cmp(entry->name, name))
			return entry;
	}

	// return an empty overflow entry
	*previous = entry;
	return &this->entries[this->overflow];
}

// Internal implementation for accepting a new property.
Slot *_props_accept_prop_internal(void *map, SepString *name, Slot *slot, bool allow_resizing) {
	PropertyMap *this = (PropertyMap*) map;
	PropertyEntry *prev, *entry = _props_find_entry(this, name, &prev);

	// is it already here?
	if (entry->name) {
		// yes, simply reassign the slot
		entry->slot = *slot;
		return &entry->slot;
	} else {
		// no, this is an empty entry to be filled
		entry->name = name;
		entry->next_entry = 0;
		entry->slot = *slot;

		// calculate the index from pointers
		uint32_t index = entry - this->entries;

		// link up with the previous entry in bucket if there is one
		if (prev)
			prev->next_entry = index;

		// did we use up overflow space?
		if (index == this->overflow) {
			this->overflow++;

			// did we run out of overflow space?
			if (this->overflow == this->capacity * 2) {
				// yes- we'd like to resize
				if (allow_resizing) {
					_props_resize(this, (int)(this->capacity * PROPERTY_MAP_GROWTH_FACTOR) + 1);
				} else {
					return NULL; // resizing disallowed
				}
			}
		}

		// find the slot again, it might have moved due to resizing
		return props_find_prop(this, name);
	}
}

// Resize the entire property map to a bigger size
void _props_resize(PropertyMap *this, int new_capacity) {
	// allocate a temporary map to handle the transfer
	PropertyMap temp;
	props_init(&temp, new_capacity);

	// reinsert existing entries into the temporary table
	// NO ALLOCATIONS are allowed during this process, as it could
	// cause the 'temp.entries' table to be freed before its copied
	// to 'this->entries'.
	PropertyIterator it = props_iterate_over(this);
	while (!propit_end(&it)) {
		bool accepted = _props_accept_prop_internal(&temp, propit_name(&it), propit_slot(&it), false);
		if (!accepted) {
			// abort mission - we have to try an even bigger size
			_props_resize(this, new_capacity * PROPERTY_MAP_GROWTH_FACTOR + 1);
			return;
		}
		propit_next(&it);
	}

	// copy data from temporary map to here, taking over the "entries" table
	this->overflow = temp.overflow;
	this->capacity = temp.capacity;
	this->entries = temp.entries;
}

// ===============================================================
//  Property maps
// ===============================================================

// Initializes an empty property map, with some initial number of entries.
void props_init(void *map, int initial_capacity) {
	PropertyMap *this = (PropertyMap*) map;

	// initial number of entries
	this->capacity = initial_capacity;

	// the number of entries are double the capacity
	// first half is for the core table (first entry in each bucket)
	// second half is for nodes in the linked list used for in-bucket
	// collisions
	size_t bytes = sizeof(PropertyEntry) * initial_capacity * 2;
	this->entries = mem_allocate(bytes);

	// the first overflow entry will be at the beginning of second half
	this->overflow = initial_capacity;

	// zero the memory so that we can recognize each entry as empty
	memset(this->entries, 0, bytes);
}

// Adds an existing slot to the map.
Slot *props_accept_prop(void *map, SepString *name, Slot *slot) {
	// delegate to the internal version
	return _props_accept_prop_internal(map, name, slot, true);
}

// Adds a new property to the map and returns the new slot stored
// inside the map. Should be preferred to props_accept_prop since
// it avoids new memory allocations.
Slot *props_add_prop(void *map, SepString *name, SlotType *slot_type, SepV initial_value) {
	Slot source_slot;
	slot_init(&source_slot, slot_type, initial_value);
	return _props_accept_prop_internal(map, name, &source_slot, true);
}

SepV props_get_prop(void *map, SepString *name) {
	PropertyMap *this = (PropertyMap*) map;
	PropertyEntry *prev, *entry = _props_find_entry(this, name, &prev);
	if (entry->name) {
		SepV host = obj_to_sepv((SepObj*)this);
		OriginInfo origin = {host, host, name};
		return slot_retrieve(&entry->slot, &origin);
	} else {
		return SEPV_NOTHING;
	}
}

// Finds the slot corresponding to a named property.
Slot *props_find_prop(void *map, SepString *name) {
	PropertyMap *this = (PropertyMap*) map;
	PropertyEntry *prev, *entry = _props_find_entry(this, name, &prev);
	if (entry->name)
		return &entry->slot;
	else
		return NULL;
}

SepV props_set_prop(void *map, SepString *name, SepV value) {
	PropertyMap *this = (PropertyMap*) map;
	PropertyEntry *prev, *entry = _props_find_entry(this, name, &prev);
	if (entry->name) {
		SepV host = obj_to_sepv((SepObj*)this);
		OriginInfo origin = {host, host, name};
		return slot_store(&entry->slot, &origin, value);
	} else {
		return SEPV_NOTHING; // this will have to be an exception in the future
	}
}

bool props_prop_exists(void *map, SepString *name) {
	PropertyMap *this = (PropertyMap*) map;
	PropertyEntry *prev, *entry = _props_find_entry(this, name, &prev);
	return (bool) (entry->name);
}

void props_add_field(void *map, const char *name, SepV value) {
	PropertyMap *this = (PropertyMap*) map;
	SepString *s_name = sepstr_for(name);
	props_add_prop(this, s_name, &st_field, value);
}

// Finds the hash table entry based on a raw hash and key string. Low-level
// functionality, mostly useful for the string cache.
PropertyEntry *props_find_entry_raw(void *map, const char *name, uint32_t hash) {
	PropertyMap *this = (PropertyMap*)map;
	// find the correct first entry for the bucket
	uint32_t index = hash % this->capacity;
	PropertyEntry *entry = &this->entries[index];

	// is this the right bucket (empty or containing the named prop?)
	if ((entry->name) && (!strcmp(entry->name->cstr, name))) {
		return entry;
	}

	// look through the linked list
	while (entry->next_entry) {
		entry = &this->entries[entry->next_entry];
		if (!strcmp(entry->name->cstr, name))
			return entry;
	}

	// nothing found
	return NULL;
}


// ===============================================================
//  Property iteration
// ===============================================================

// Starts a new iteration over all the properties.
PropertyIterator props_iterate_over(void *map) {
	PropertyMap *this = (PropertyMap*) map;
	PropertyIterator it = { this, this->entries - 1 };
	propit_next(&it);
	return it;
}
// Move to the next property.
void propit_next(PropertyIterator *current) {
	PropertyEntry *end = current->map->entries + (current->map->overflow);
	current->entry++;
	while ((current->entry < end) && (!current->entry->name))
		current->entry++;
}
// Check if the iterator reached the end. If this is true,
// no other iterator methods can be called.
bool propit_end(PropertyIterator *current) {
	return (current->entry - current->map->entries) >= (current->map->overflow);
}
// The name of the current property.
SepString *propit_name(PropertyIterator *current) {
	return current->entry->name;
}
// The slot representing the property.
Slot *propit_slot(PropertyIterator *current) {
	return &(current->entry->slot);
}
// The value of the property.
SepV propit_value(PropertyIterator *current) {
	Slot *slot = &(current->entry->slot);
	SepV host = obj_to_sepv((SepObj*)current->map);
	OriginInfo origin = {host, host, current->entry->name};
	return slot_retrieve(slot, &origin);
}

// ===============================================================
//  C3 property resolution
// ===============================================================

SepArray *c3_order(SepV object_v, SepV *error);

void c3_invalidate_cache(SepObj *object) {
	Slot *cache_slot = props_find_prop(object, sepstr_for("<c3>"));
	if (cache_slot)
		cache_slot->value = SEPV_NO_VALUE;
}

void c3_cache_order(SepV object_v, SepArray *order) {
	if (!sepv_is_obj(object_v))
		return;

	SepObj *object = sepv_to_obj(object_v);
	obj_add_field(object, "<c3>", obj_to_sepv(order));
}

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

// Calculates the C3 property resolution order for an object based
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
	c3_cache_order(object_v, order);
	return order;
}

// ===============================================================
//  Objects
// ===============================================================

// Creates a new, empty object.
SepObj *obj_create() {
	static ObjectTraits DEFAULT_TRAITS = { REPRESENTATION_SIMPLE };

	SepObj *obj = mem_allocate(sizeof(SepObj));

	// set up default values
	obj->traits = DEFAULT_TRAITS;
	obj->prototypes = obj_to_sepv(rt.Object);

	// make sure all unallocated pointers are NULL to avoid GC
	// tripping over uninitialized pointers and going berserk on
	// random memory
	obj->data = NULL;
	obj->props.entries = NULL;

	// register in as a GC root in the current frame to prevent accidental freeing
	gc_register(obj_to_sepv(obj));

	// initialize property map (possible allocation here, so perform as late as possible)
	props_init((PropertyMap*) obj, 2);

	return obj;
}

// Creates a new object with a chosen prototype(s).
SepObj *obj_create_with_proto(SepV proto) {
	SepObj *obj = obj_create();
	obj->prototypes = proto;
	return obj;
}

// Resets an object's prototype list, throwing away any outstanding caches
// resulting from the old one.
void obj_set_prototypes(SepObj *this, SepV prototypes) {
	this->prototypes = prototypes;
	c3_invalidate_cache(this);
}

// Shortcut to quickly create a SepItem with a given object as r-value.
SepItem si_obj(void *object) {
	return item_rvalue(obj_to_sepv(object));
}


// ===============================================================
//  Object-like behavior for all types
// ===============================================================

// Finds the prototype list (or single prototype) for a given SepV,
// handling both rich objects and primitives.
SepV sepv_prototypes(SepV sepv) {
	// return based on types
	switch (sepv & SEPV_TYPE_MASK) {
	// object?
	case SEPV_TYPE_OBJECT:
	case SEPV_TYPE_EXCEPTION:
		return sepv_to_obj(sepv)->prototypes;

		// primitive?
	case SEPV_TYPE_INT:
		return obj_to_sepv(rt.Integer);

	case SEPV_TYPE_STRING:
		return obj_to_sepv(rt.String);

	case SEPV_TYPE_FUNC:
		return obj_to_sepv(rt.Function);

	case SEPV_TYPE_SLOT:
		return obj_to_sepv(rt.Slot);

		// special?
	case SEPV_TYPE_SPECIAL:
		if ((sepv == SEPV_TRUE) || (sepv == SEPV_FALSE))
			return obj_to_sepv(rt.Bool);
		if (sepv == SEPV_NOTHING)
			return obj_to_sepv(rt.NothingType);
		return SEPV_NOTHING;

		// we don't handle this yet, so no prototypes for you
		// this clause will disappear once all types are handled
	default:
		return SEPV_NOTHING;
	}
}

Slot *sepv_local_lookup(SepV sepv, SepString *property, SepV *owner_ptr) {
	// if we are an object, we might have this property
	if (sepv_is_obj(sepv)) {
		SepObj *obj = sepv_to_obj(sepv);
		Slot *local_slot = props_find_prop(obj, property);
		if (local_slot) {
			// found it!
			if (owner_ptr)
				*owner_ptr = sepv;
			return local_slot;
		}
	}

	// nothing locally
	return NULL;
}

// Finds a property starting from a given object, taking prototypes
// into consideration. Returns NULL if nothing found.
// If the 'owner_ptr' is non-NULL, it will also write the actual
// 'owner' of the slot (i.e. the prototype in which the property
// was finally found) into the memory being pointed to.
Slot *sepv_lookup(SepV sepv, SepString *property, SepV *owner_ptr, SepV *error) {
	SepV err = SEPV_NO_VALUE;

	// handle the LiteralScope in a special way
	if (sepv == SEPV_LITERALS) {
		// return the property name itself, wrapped in a fake slot
		*owner_ptr = SEPV_NO_VALUE;
		return slot_create(&st_field, str_to_sepv(property));
	}

	// check locally first
	Slot *local_slot = sepv_local_lookup(sepv, property, owner_ptr);
	if (local_slot)
		return local_slot;

	// check 'syntax' object next if we are an execution scope
	ExecutionFrame *current_frame = vm_current_frame();
	if (rt.syntax && current_frame && (current_frame->locals == sepv)) {
		Slot *syntax_slot = sepv_local_lookup(obj_to_sepv(rt.syntax), property, owner_ptr);
		if (syntax_slot)
			return syntax_slot;
	}

	// find the C3 lookup order
	SepArray *lookup_order = c3_order(sepv, &err);
		or_fail_with(NULL);

	// look into the objects in turn (skipping the first entry, which is just us again)
	SepArrayIterator it = array_iterate_over(lookup_order);
	arrayit_next(&it);
	while (!arrayit_end(&it)) {
		SepV obj = arrayit_next(&it);
		Slot *slot = sepv_local_lookup(obj, property, owner_ptr);
		if (slot)
			return slot;
	}

	// nothing found
	return NULL;
}

// Gets the value of a property from an arbitrary SepV, using
// proper lookup procedure.
SepItem sepv_get_item(SepV sepv, SepString *property) {
	SepV err = SEPV_NO_VALUE;
	SepV owner;
	Slot *slot = sepv_lookup(sepv, property, &owner, &err);
		or_raise(err);

	if (slot) {
		OriginInfo origin = {sepv, owner, property};
		SepV value = slot_retrieve(slot, &origin);
		if (owner != SEPV_NO_VALUE) {
			// just a normal property
			return item_property_lvalue(owner, sepv, property, slot, value);
		} else {
			// SEPV_NO_VALUE in the owner means that
			// this is an artificially-created slot
			return item_artificial_lvalue(slot, value);
		}
	} else {
		SepString *message = sepstr_sprintf("Property '%s' does not exist.",
				property->cstr);
		return si_exception(exc.EMissingProperty, message);
	}
}

// Gets the value of a property from an arbitrary SepV, using
// proper lookup procedure. If the property is found, returns
// it in the SepV. If the property is absent, SEPV_NO_VALUE will
// be returned as a marker. If you prefer an exception, use
// sepv_get().
SepV sepv_lenient_get(SepV sepv, SepString *property) {
	SepV err = SEPV_NO_VALUE;
	SepV owner;
	Slot *slot = sepv_lookup(sepv, property, &owner, &err);
		or_raise_sepv(err);
	if (slot) {
		OriginInfo origin = {sepv, owner, property};
		return slot_retrieve(slot, &origin);
	} else {
		return SEPV_NO_VALUE;
	};
}

// Gets the value of a property from an arbitrary SepV, using
// proper lookup procedure. Returns just a SepV.
SepV sepv_get(SepV sepv, SepString *property) {
	SepV value = sepv_lenient_get(sepv, property);
	if (value == SEPV_NO_VALUE) {
		return sepv_exception(exc.EMissingProperty,
				sepstr_sprintf("Property '%s' does not exist.", property->cstr));
	} else {
		return value;
	}
}

// Takes a SepV that will be called, and returns the proper SepFunc*
// that will implement that call. For SepVs that are functions, this
// will be the function itself. For objects, the special "<call>"
// property will be used. If the SepV is not callable, NULL will be
// returned.
SepFunc *sepv_call_target(SepV value) {
	// do we have a function yet?
	if (sepv_is_func(value))
		return sepv_to_func(value);

	if (!property_exists(value, "<call>"))
		return NULL;

	// recurse into the <call> property
	SepV call_property = property(value, "<call>");
	return sepv_call_target(call_property);
}
