#ifndef _SEP_OBJECTS_H_
#define _SEP_OBJECTS_H_

/*****************************************************************
 **
 ** runtime/objects.h
 **
 ** Defines the SepObj - the runtime representation of every object
 ** manipulated by September code.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdint.h>
#include "types.h"
#include "strings.h"
#include "mem.h"
#include "stack.h"

// ===============================================================
//  Pre-declaring structs
// ===============================================================

struct SepObj;
struct Slot;
struct SlotVTable;
struct PropertyEntry;
struct PropertyMap;
struct SepFunc;

// ===============================================================
//  Slots
// ===============================================================

/**
 * A slot is a container that can store a single value. The slot
 * couples the value with a behavior that triggers when the value
 * is stored and retrieved. This allows us to have methods behave
 * differently to fields when retrieved, for example.
 */

/**
 * Every slot points to a v-table with two methods that control how
 * values are stored and retrieved.
 */
typedef struct SlotVTable {
	SepV (*retrieve)(struct Slot *slot, SepV context);
	SepV (*store)(struct Slot *slot, SepV context, SepV newValue);
} SlotVTable;

/**
 * The slot structure.
 */
typedef struct Slot {
	// defines the behavior of this slot (as a field,
	// a method, etc.)
	SlotVTable *vt;
	// the value stored in the slot (or in the case of
	// custom slots, the slot object)
	SepV value;
} Slot;

// Create a new slot with specified behavior and initial value
Slot *slot_create(SlotVTable *behavior,
		SepV initial_value);

// Creates a new 'field'-type slot.
Slot *field_create(SepV initial_value);
// Creates a new 'method'-type slot.
Slot *method_create(SepV initial_value);

// Convert slots to SepV and back
#define slot_to_sepv(slot) ((SepV)(((intptr_t)(slot) >> 3) | SEPV_TYPE_SLOT))
#define sepv_to_slot(sepv) ((Slot*)(intptr_t)(sepv << 3))

// ===============================================================
//  Property maps
// ===============================================================

/**
 * Property maps group slots into a hashmap, which forms the basis
 * of September objects.
 */

/**
 * A single entry in the property hashmap.
 */
typedef struct PropertyEntry {
	// the index of the next entry in this bucket,
	// or 0 if this is the last one
	uint32_t		next_entry;
	// the name of the property
	SepString		*name;
	// the slot for this property
	Slot			slot;
} PropertyEntry;

typedef struct PropertyMap {
	// the number of buckets in the hashmap
	uint32_t capacity;
	// the overflow pointer - points to the first free
	// "overflow" entry for "second-in-bucket" elements
	uint32_t overflow;
	// the data table - always sized as double the capacity
	PropertyEntry *entries;
} PropertyMap;

/**
 * All methods take a void* in order to make it possible to use
 * them on SepObj* as well as a PropertyMap* (SepObj begins with
 * a property map struct).
 */

// Initializes an empty property map, with some initial 'capacity'.
void props_init(void *this,
		int initial_capacity);

// Adds a new property to the map and returns the new slot stored
// inside the map.
Slot *props_accept_prop(void *this, SepString *name,
		Slot *slot);
// Removes a previously existing property.
// TODO: implement SepV props_delete_prop(void *this, SepString *name);
// Sets the value of a property, which must already exist in the map.
SepV props_set_prop(void *this, SepString *name, SepV value);
// Gets the value of a property, which must exist in the map.
SepV props_get_prop(void *this, SepString *name);
// Finds the slot corresponding to a named property.
Slot *props_find_prop(void *this, SepString *name);
// Checks whether a property exists or not.
bool props_prop_exists(void *this, SepString *name);

// Convenience method for quickly adding hard-coded fields.
void props_add_field(void *this, const char *name,
		SepV value);

// Finds the hash table entry based on a raw hash and key string. Low-level
// functionality, mostly useful for the string cache.
PropertyEntry *props_find_entry_raw(void *this, const char *name, uint32_t hash);

// ===============================================================
//  Property iteration
// ===============================================================

/**
 * Property iterator allows iterating over all properties in a map.
 * As it is a hashmap, the order of iteration is basically random.
 */
typedef struct PropertyIterator {
	// the map we are iterating over
	PropertyMap *map;
	// the entry we are currently on
	PropertyEntry *entry;
} PropertyIterator;

// Starts a new iteration over all the properties.
PropertyIterator props_iterate_over(void *this);
// Move to the next property.
void propit_next(PropertyIterator *current);
// Check if the iterator reached the end. If this is true,
// no other iterator methods can be called.
bool propit_end(PropertyIterator *current);
// The name of the current property.
SepString *propit_name(PropertyIterator *current);
// The slot representing the property.
Slot *propit_slot(PropertyIterator *current);
// The value of the property.
SepV propit_value(PropertyIterator *current);

// ===============================================================
//  Objects
// ===============================================================

/**
 * Each object carries a 'traits', which is a bit-struct with various
 * metadata about the object. Currently, the most important bit is
 * the internal representation (SepObj or SepArray).
 */
enum ObjectRepresentation {
	// object is represented by a SepObj
	REPRESENTATION_SIMPLE = 0,
	// object is represented by a SepArray
	REPRESENTATION_ARRAY = 1
};
typedef struct ObjectTraits {
	unsigned int representation : 1;
} ObjectTraits;

/**
 * One of the core types used by September, represents a hash-like object.
 * The object is basically synonymous with a hashmap with some additional
 * bookkeeping values and a defined allocation strategy.
 */
typedef struct SepObj {
	// the hashmap storing all the properties of this object
	PropertyMap props;

	// The prototypes this objects inherits from
	// This will either be an object in case of a single prototype,
	// or an array if there are multiple. This duality is intended
	// to save memory in the common case.
	SepV prototypes;

	// a set of flags describing the object
	ObjectTraits traits;

	// a space for additional data in the form of a C struct
	// usually used by C methods of a prototype
	void *data;
} SepObj;

// Creates a new, empty object.
SepObj *obj_create();
// Creates a new object with a chosen prototype(s).
SepObj *obj_create_with_proto(SepV proto);
// Shortcut to quickly create a SepItem with a given object as r-value.
SepItem si_obj(void *object);

// ===============================================================
//  Property lookup for all types
// ===============================================================

// Finds a property starting from a given object, taking prototypes
// into consideration. Returns NULL if nothing is found. For
// primitive types, lookup starts with their prototype.
Slot *sepv_lookup(SepV object, SepString *property);
// Gets the value of a property from an arbitrary SepV, using
// proper lookup procedure, and returning a stack item (slot + its value).
SepItem sepv_get_item(SepV object, SepString *property);
// Gets the value of a property from an arbitrary SepV, using
// proper lookup procedure. Returns just a SepV.
SepV sepv_get(SepV object, SepString *property);
// Takes a SepV that will be called, and returns the proper SepFunc*
// that will implement that call. For SepVs that are functions, this
// will be the function itself. For objects, the special "<call>"
// property will be used. If the SepV is not callable, NULL will be
// returned.
struct SepFunc *sepv_call_target(SepV object);

// ===============================================================
//  Macros for working with objects
// ===============================================================

#define obj_to_sepv(obj) ((obj) ? (SEPV_TYPE_OBJECT | (((intptr_t)(obj)) >> 3)) : SEPV_NOTHING)
#define sepv_to_obj(val) ((val == SEPV_NOTHING) ? NULL : ((SepObj*)(((intptr_t)val) << 3)))
#define sepv_is_obj(val) ((val & SEPV_TYPE_MASK) == SEPV_TYPE_OBJECT)

#define obj_is_simple(obj) (((SepObj*)(obj))->traits.representation == REPRESENTATION_SIMPLE)
#define obj_is_array(obj) (((SepObj*)(obj))->traits.representation == REPRESENTATION_ARRAY)

#define sepv_is_array(val) (sepv_is_obj(val) && obj_is_array(sepv_to_obj(val)))
#define sepv_is_simple_object(val) (sepv_is_obj(val) && obj_is_simple(sepv_to_obj(val)))
#define sepv_to_array(val) ((SepArray*)(sepv_to_obj(val)))

/*****************************************************************/

#endif /* OBJECTS_H_ */
