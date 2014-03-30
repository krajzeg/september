/*****************************************************************
 **
 ** stack.c
 **
 ** Implementation of the data stack.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>

#include "exceptions.h"
#include "stack.h"

#include "../common/debugging.h"
#include "../vm/runtime.h"

// ===============================================================
//  Stack implementation
// ===============================================================

// Creates a new, empty data stack.
SepStack *stack_create() {
	SepStack *stack = (SepStack*)mem_unmanaged_allocate(sizeof(SepStack));
	ga_init(&stack->array, 8, sizeof(SepItem), &allocator_unmanaged);
	return stack;
}

// Frees the stack and all the contents.
void stack_free(SepStack *this) {
	mem_unmanaged_free(this);
}

// == stack operation

// Checks if the stack is empty.
bool stack_empty(SepStack *this) {
	return ga_length(&this->array) == 0;
}

// Pushes a new SepItem (slot + value) on the stack.
void stack_push_item(SepStack *this, SepItem item) {
	log0("stack", "Pushed.");
	ga_push(&this->array, &item);
}

// Pops an item from the stack, with exception on empty stack.
SepItem stack_pop_item(SepStack *this) {
	SepItem *ptr = ga_pop(&this->array);
	if (!ptr) {
		SepV exception = sepv_exception(exc.EInternal, sepstr_for("Internal error: stack underflow."));
		return item_rvalue(exception);
	}
	log0("stack", "Popped.");
	return *ptr;
}

// Returns the top item from the stack, with exception on empty stack.
SepItem stack_top_item(SepStack *this) {
	uint32_t length = ga_length(&this->array);
	if (!length) {
		SepV exception = sepv_exception(exc.EInternal, sepstr_for("Internal error: stack underflow."));
		return item_rvalue(exception);
	}
	return *(SepItem*)ga_get(&this->array, length-1);
}

// === shortcuts

// Pushes an rvalue (value without a slot) on the stack.
void stack_push_rvalue(SepStack *this, SepV value) {
	SepItem item = {NULL, value};
	stack_push_item(this, item);
}

// Pushes a new item on the stack by joining a slot and its value.
void stack_push_lvalue(SepStack *this, Slot *origin, SepV value) {
	SepItem item = {origin, value};
	stack_push_item(this, item);
}
// Pops just the value from the top of the stack.
SepV stack_pop_value(SepStack *this) {
	SepItem item = stack_pop_item(this);
	return item.value;
}

// Returns the value from the top item on the stack.
SepV stack_top_value(SepStack *this) {
	SepItem item = stack_top_item(this);
	return item.value;
}

// === function calls

// Starts a new argument list on the stack. Once it is started, you MUST
// add the specified number of arguments through stack_next_argument, then
// close the list with stack_end_argument_list before the stack can operate
// normally again.
void stack_start_argument_list(SepStack *this, uint8_t argument_count) {
	// reserve some space
	uint32_t end_index = ga_length(&this->array);
	ga_grow(&this->array, argument_count+1);

	// put END_ARGUMENTS at the right place on the stack
	SepItem argument_list_marker = {NULL, SEPV_END_ARGUMENTS};
	ga_set(&this->array, end_index, &argument_list_marker);

	// the next argument to be added will be the 0th
	this->arglist_index = 0;
}

// Adds the next argument to a started argument list.
void stack_next_argument(SepStack *this, SepItem argument_value) {
	uint32_t index = ga_length(&this->array) - 1 - this->arglist_index;
	ga_set(&this->array, index, &argument_value);
}

// Finalizes the argument list being built on the stack.
void stack_end_argument_list(SepStack *this) {
	// nothing in this implementation, but might be
	// needed later
}


