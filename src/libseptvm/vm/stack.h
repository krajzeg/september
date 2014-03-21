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

// === shortcuts

// Pushes an rvalue (value without a slot) on the stack.
void stack_push_rvalue(SepStack *this, SepV value);
// Pushes a new item on the stack by joining a slot and its value.
void stack_push_lvalue(SepStack *this, struct Slot *origin, SepV value);
// Pops just the value from the top of the stack.
SepV stack_pop_value(SepStack *this);
// Returns the value from the top item on the stack.
SepV stack_top_value(SepStack *this);

// === function calls

// Starts a new argument list on the stack. Once it is started, you MUST
// add the specified number of arguments through stack_next_argument, then
// close the list with stack_end_argument_list before the stack can operate
// normally again.
void stack_start_argument_list(SepStack *this, uint8_t argument_count);
// Adds the next argument to a started argument list.
void stack_next_argument(SepStack *this, SepItem argument_value);
// Finalizes the argument list being built on the stack.
void stack_end_argument_list(SepStack *this);

/*****************************************************************/

#endif
