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

// Replaces the top item on the stack with another item.
SepV stack_replace_top(SepStack *this, SepItem new_item) {
	uint32_t length = ga_length(&this->array);
	if (!length) {
		return sepv_exception(exc.EInternal, sepstr_for("Internal error: stack underflow."));
	}
	ga_set(&this->array, length-1, &new_item);
	return SEPV_NOTHING;
}

// === shortcuts

// Pushes an rvalue (value without a slot) on the stack.
void stack_push_rvalue(SepStack *this, SepV value) {
	stack_push_item(this, item_rvalue(value));
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
