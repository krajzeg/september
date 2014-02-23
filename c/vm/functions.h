#ifndef _SEP_FUNCTIONS_H
#define _SEP_FUNCTIONS_H

/*****************************************************************
 **
 ** runtime/functions.h
 **
 ** Support for executable functions, including built-in functions
 ** (written in C) and September functions.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdarg.h>
#include <stdint.h>
#include "types.h"
#include "objects.h"
#include "code.h"

// ===============================================================
//  Pre-declaring structs
// ===============================================================

struct ExecutionFrame;
struct SepFunc;

// ===============================================================
//  Common function interface
// ===============================================================

typedef struct FuncParam {
	// undecorated parameter name
	SepString *name;
	// style
	struct {
		int lazy:1;
	} flags;
} FuncParam;

typedef struct SepFuncVTable {
	// initializes an execution frame, setting the instruction pointer
	void (*initialize_frame)(struct SepFunc *this, struct ExecutionFrame *frame);
	// executes instructions from this function, up to a limit
	int (*execute_instructions)(struct SepFunc *this, struct ExecutionFrame *frame, int limit);
	// returns the parameter count for this function
	uint8_t (*get_parameter_count)(struct SepFunc *this);
	// returns the parameter array for this function
	FuncParam *(*get_parameters)(struct SepFunc *this);
	// returns the scope in which this function was first declared - this is the
	// closure scope
	SepV (*get_declaration_scope)(struct SepFunc *this);
	// in case of methods, return the 'this' object a method instance is bound to
	// for free functions, this will simply be NULL
	SepV (*get_this_pointer)(struct SepFunc *this);
} SepFuncVTable;

typedef struct SepFunc {
	// v-table that controls actual behavior
	SepFuncVTable *vt;
} SepFunc;

// ===============================================================
//  Built-in (C) functions
// ===============================================================

typedef SepItem (*BuiltInImplFunc)(SepObj *scope, struct ExecutionFrame *frame);

typedef struct BuiltInFunc {
	// common interface
	struct SepFunc base;
	// C implementation
	BuiltInImplFunc implementation;
	// parameters accepted by this function
	uint8_t parameter_count;
	FuncParam *parameters;
	// additional data associated with this function
	void *data;
} BuiltInFunc;

// Creates a new built-in based on a C function and September parameter names.
BuiltInFunc *builtin_create(BuiltInImplFunc implementation, uint8_t parameters, ...);
// Creates a new built-in based on a C function and September parameter names.
// This version receives parameters as a pre-started va_list to allow use from
// other var-arg functions.
BuiltInFunc *builtin_create_va(BuiltInImplFunc implementation, uint8_t parameters, va_list args);

// ===============================================================
//  Interpreted (September) functions
// ===============================================================

/**
 * A type of SepFunc which consists of actual September code. The logic for
 * the function is inside a code block.
 */
typedef struct InterpretedFunc {
	// common interface
	struct SepFunc base;
	// code
	CodeBlock *block;
	// declaration scope
	SepV declaration_scope;
} InterpretedFunc;

// Creates a new interpreted function for a given piece of code.
InterpretedFunc *ifunc_create(CodeBlock *block, SepV declaration_scope);

// ===============================================================
//  Bound method objects
// ===============================================================

typedef struct BoundMethod {
	// common interface
	struct SepFunc base;
	// original SepFunc for the free function behind this method
	SepFunc *original_instance;
	// the current 'this' pointer we are bound to
	SepV this_pointer;
} BoundMethod;

// Creates a new bound method based on a given free function.
BoundMethod *boundmethod_create(SepFunc *function, SepV this_pointer);

// ===============================================================
//  SepV conversions
// ===============================================================

#define func_to_sepv(slot) ((SepV)(((intptr_t)(slot) >> 3) | SEPV_TYPE_FUNC))
#define sepv_to_func(sepv) ((SepFunc*)(intptr_t)(sepv << 3))
#define sepv_is_func(sepv) ((sepv & SEPV_TYPE_MASK) == SEPV_TYPE_FUNC)

/*****************************************************************/

#endif
