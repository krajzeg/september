#ifndef _SEP_STACK_H_
#define _SEP_STACK_H_

/*****************************************************************
 **
 ** vm/stack.h
 **
 ** Contains everything related to the data stack which is used
 ** by the virtual machine.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdbool.h>
#include "types.h"
#include "../common/garray.h"

// ===============================================================
//  The data stack
// ===============================================================

typedef struct SepStack {
	// the underlying generic array
	GenericArray array;

	// index of the next argument to be added to the argument list
	int32_t arglist_index;
} SepStack;

// == (de)allocation

// Creates a new, empty data stack.
SepStack *stack_create();
// Frees the stack and all the contents.
void stack_free(SepStack *this);

// == stack operation

// Checks if the stack is empty.
bool stack_empty(SepStack *this);
// Pushes a new SepItem (slot + value) on the stack.
void stack_push_item(SepStack *this, SepItem item);
// Pops an item from the stack, with exception on empty stack.
SepItem stack_pop_item(SepStack *this);
// Returns the top item from the stack, with exception on empty stack.
SepItem stack_top_item(SepStack *this);
// Replaces the top item on the stack with another item. Returns exception on failure (empty stack).
SepV stack_replace_top(SepStack *this, SepItem new_item);

// === shortcuts

// Pushes an rvalue (value without a slot) on the stack.
void stack_push_rvalue(SepStack *this, SepV value);
// Pops just the value from the top of the stack.
SepV stack_pop_value(SepStack *this);
// Returns the value from the top item on the stack.
SepV stack_top_value(SepStack *this);

/*****************************************************************/

#endif
