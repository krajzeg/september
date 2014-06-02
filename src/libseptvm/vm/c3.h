#ifndef C3_H_
#define C3_H_

/*****************************************************************
 **
 ** c3.h
 **
 ** Functions for dealing with the C3 property resolution order used
 ** in September objects.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes and predefinitions
// ===============================================================

#include "types.h"

struct SepArray;

// ===============================================================
//  C3 public interface
// ===============================================================

// Returns the C3 resolution order for the given object. The returned
// array will include all direct and indirect prototypes, sorted according
// to the C3 rules. If it's impossible to resolve an unambiguous order,
// an error will be raised.
struct SepArray *c3_order(SepV object_v, SepV *error);

// Returns the version number of the cached C3 order stored within an
// object, or 0 if there isn't one. These version numbers can be used
// to detect when the cache has to be invalidated due to prototype
// changes.
int64_t c3_cache_version(SepV object_v);

// Invalidates the internally cached C3 order stored within the object,
// causing it to be recalculated on next property access.
void c3_invalidate_cache(SepV object_v);

/*****************************************************************/

#endif
