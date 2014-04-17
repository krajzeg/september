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
#include "opcodes.h"
#include "types.h"
#include "objects.h"
#include "funcparams.h"

// ===============================================================
//  Pre-declaring structs
// ===============================================================

struct ArgumentSource;
struct ExecutionFrame;
struct SepFunc;
struct SepModule;

// ===============================================================
//  Common function interface
// ===============================================================

struct GarbageCollection;
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
	// marks any managed memory regions allocated by this function and
	// queues any objects reachable from this SepFunc instance for marking
	// in garbage collection
	void (*mark_and_queue)(struct SepFunc *this, struct GarbageCollection *gc);
} SepFuncVTable;

typedef struct SepFunc {
	// the module which hosts this function
	struct SepModule *module;
	// v-table that controls actual behavior
	SepFuncVTable *vt;
	// is this a lazy closure?
	bool lazy;
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
	SepV data;
	void *additional_pointer;
} BuiltInFunc;

// Creates a new built-in based on a C function and September parameter names.
BuiltInFunc *builtin_create(BuiltInImplFunc implementation, uint8_t parameters, ...);
// Creates a new built-in based on a C function and September parameter names.
// This version receives parameters as a pre-started va_list to allow use from
// other var-arg functions.
BuiltInFunc *builtin_create_va(BuiltInImplFunc implementation, uint8_t parameters, va_list args);

// ===============================================================
//  Code blocks
// ===============================================================

/**
 * Represents a single code block from a module file. Those are just
 * the instructions - to get something callable, the block is wrapped
 * in an InterpretedFunc. Many function instances can point to the same
 * block, allowing for function instances with the same logic, but
 * different scopes/properties (which is how closures work).
 */
typedef struct CodeBlock {
	// pointer to the module this code comes from
	struct SepModule *module;
	// parameter count
	uint8_t parameter_count;
	// parameter array
	struct FuncParam *parameters;
	// block of memory containing instructions, packed tightly
	CodeUnit *instructions;
	// pointer to the space right after the last instruction in this block
	CodeUnit *instructions_end;
} CodeBlock;

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
//  Lazy closures
// ===============================================================

/**
 * Lazy closures are implemented as interpreted functions, but with
 * a different v-table address to make them recognizable.
 */

// Creates a new lazy closure for a given expression.
InterpretedFunc *lazy_create(CodeBlock *block, SepV declaration_scope);
// Checks whether a given SepV is a lazy closure.
bool sepv_is_lazy(SepV sepv);
// Checks whether a given SepFunc* is a lazy closure.
bool func_is_lazy(SepFunc *func);

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
