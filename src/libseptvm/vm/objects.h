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
struct SlotType;
struct PropertyEntry;
struct PropertyMap;
struct SepFunc;
struct GarbageCollection;

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
typedef struct SlotType {
	// special flags enabling very specific handling in the VM (see SlotFlag enum)
	// for most slot types, this will be 0 - unless magic is needed
	uint8_t flags;
	// retrieves the value from the slot, taking the slot's owner
	// and the host object into account
	SepV (*retrieve)(struct Slot *slot, struct OriginInfo *origin);
	// stores a new value into the slot
	SepV (*store)(struct Slot *slot, struct OriginInfo *origin, SepV newValue);
	// optional - marks and queues any dependent objects during GC
	// NULL if not required
	void (*mark_and_queue)(struct Slot *slot, struct GarbageCollection *gc);
} SlotType;

/**
 * Flags for slot types.
 */
typedef enum SlotFlag {
	// used by st_magic_word - if this slot is ever popped explicitly from the stack,
	// the function inside the slot gets executed
	SF_MAGIC_WORD = 0x1,

	// value used when the slot doesn't want any special treatment
	SF_NOTHING_SPECIAL = 0x0
} SlotFlag;

/**
 * The slot structure.
 */
typedef struct Slot {
	// defines the behavior of this slot (as a field,
	// a method, etc.)
	SlotType *vt;
	// the value stored in the slot (or in the case of
	// custom slots, the slot object)
	SepV value;
} Slot;

// Create a new slot with specified behavior and initial value
Slot *slot_create(SlotType *behavior,
		SepV initial_value);
// Retrieves a value from any type of slot.
SepV slot_retrieve(Slot *slot, OriginInfo *origin);
// Stores a value in any type of slot.
SepV slot_store(Slot *slot, OriginInfo *origin, SepV new_value);

// Convert slots to SepV and back
#define slot_to_sepv(slot) ((SepV)(((intptr_t)(slot) >> 3) | SEPV_TYPE_SLOT))
#define sepv_to_slot(sepv) ((Slot*)(intptr_t)(sepv << 3))
#define sepv_is_slot(sepv) (sepv_type(sepv) == SEPV_TYPE_SLOT)

// ===============================================================
//  Specific slot types
// ===============================================================

// Built-in slot types
// Simple dumb field for any type of value.
extern SlotType st_field;
// Method slot - binds the function with its "this" pointer on retrieval.
extern SlotType st_method;
// Special slot for pseudo-keywords like "return" or "break". The function
// inside the slot is called whenever it appears as the only value in an
// expression.
extern SlotType st_magic_word;

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
// inside the map. Should be preferred to props_accept_prop since
// it avoids new memory allocations.
Slot *props_add_prop(void *this, SepString *name,
		SlotType *slot_type, SepV initial_value);
// Copies an existing slot into the map and returns the new slot stored
// inside the map. The original can be thrown away.
Slot *props_accept_prop(void *this, SepString *name,
		Slot *slot);
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

	// a space for arbitrary additional data used by the C side
	// if this pointer is non-null, it must be:
	// a) allocated from managed memory
	// b) not hold any references for GC or other outside pointers
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

// Finds the prototype list (or single prototype) for a given SepV,
// handling both rich objects and primitives.
SepV sepv_prototypes(SepV sepv);
// Finds a property starting from a given object, taking prototypes
// into consideration. Returns NULL if nothing is found. For
// primitive types, lookup starts with their prototype.
// If 'owner_ptr' is non-NULL, we will also write the actual
// 'owner' of the slot (i.e. the prototype in which the property
// was finally found) through this pointer.
Slot *sepv_lookup(SepV object, SepString *property, SepV *owner_ptr);
// Gets the value of a property from an arbitrary SepV, using
// proper lookup procedure, and returning a stack item (slot + its value).
SepItem sepv_get_item(SepV object, SepString *property);
// Gets the value of a property from an arbitrary SepV, using
// proper lookup procedure. Returns just a SepV.
SepV sepv_get(SepV object, SepString *property);
// A variant of sepv_get() that returns SEPV_NO_VALUE when the property
// is missing, instead of throwing exceptions.
SepV sepv_lenient_get(SepV sepv, SepString *property);

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
