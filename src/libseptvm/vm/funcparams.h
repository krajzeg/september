#ifndef FUNCPARAMS_H_
#define FUNCPARAMS_H_

/*****************************************************************
 **
 ** vm/funcparams.h
 **
 ** Encapsulates all functionality dealing with how function parameters
 ** are represented, and how function call arguments end up in the
 ** callee's execution scope.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes and pre-declarations
// ===============================================================

#include <stddef.h>
#include <stdarg.h>
#include "opcodes.h"
#include "types.h"
#include "arrays.h"

struct ExecutionFrame;
struct SepFunc;
struct SepObj;

// ===============================================================
//  Function parameters
// ===============================================================

/**
 * Types of parameters.
 */
typedef enum ParamType {
	// a standard single-valued parameter
	PT_STANDARD_PARAMETER = 0,
	// an array-like sink parameters - any superfluous positional arguments
	// end up in such a parameter
	PT_POSITIONAL_SINK = 1,
	// a map-like sink parameter - any superfluous named arguments
	// that cannot be matched to other parameters end up in
	// such a a parameter
	PT_NAMED_SINK = 2
} ParamType;

/**
 * The parameter structure.
 */
typedef struct FuncParam {
	// undecorated parameter name (without parameter-type and laziness sigils)
	struct SepString *name;
	// flags controlling how the parameter works
	struct {
		// is the argument lazily evaluated?
		int lazy:1;
		ParamType type:2;
		// is it optional? if it is, it has to have a default
		int optional:1;
	} flags;
	// if the parameter is optional, a reference to the default value
	// (an index into the constant pool/function pool) will be stored
	// here (otherwise uninitialized)
	CodeUnit default_value_reference;
} FuncParam;

// ===============================================================
//  Argument sources
// ===============================================================

/**
 * Represents a single argument passed through a function call.
 */
typedef struct Argument {
	// the name that was provided for the argument, or NULL for
	// positional arguments
	SepString *name;
	// the value of the argument
	SepV value;
} Argument;

/**
 * An argument source is a place from which arguments for a function
 * call are gathered. There are 3 ways to do it:
 * - opcode LAZY - arguments come from bytecode
 * - vm_subcall_va - arguments come from a C vararg list
 * - vm_subcall_array - arguments come from a SepArray
 * Hence, a polymorphic iterator interface for getting them was needed.
 */
struct ArgumentSourceVT;
typedef struct ArgumentSource {
	struct ArgumentSourceVT *vt;
} ArgumentSource;

typedef uint8_t argcount_t;
typedef struct ArgumentSourceVT {
	// returns an argument and advances the source to the next one
	// once we run out of arguments, this function returns NULL
	Argument *(*get_next_argument)(ArgumentSource *);
} ArgumentSourceVT;

// ===============================================================
//  Bytecode argument source
// ===============================================================

/**
 * Argument source for getting arguments stored in bytecode.
 */
typedef struct BytecodeArgs {
	struct ArgumentSource base;
	// source for arguments
	struct ExecutionFrame *source_frame;
	// stored argument index and count
	argcount_t argument_index, argument_count;
	// a place for the argument that get_next_argument returns
	Argument current_arg;
} BytecodeArgs;

// Initializes a new bytecode source in place.
void bytecodeargs_init(BytecodeArgs *this, struct ExecutionFrame *frame);

// ===============================================================
//  va_list argument source
// ===============================================================

/**
 * Argument source for getting arguments from a C vararg function.
 */
typedef struct VAArgs {
	struct ArgumentSource base;
	// stored argument index and count
	argcount_t argument_index, argument_count;
	// a started va_list with the arguments as SepVs
	va_list c_arg_list;
	// a place for the argument that get_next_argument returns
	Argument current_arg;
} VAArgs;

// Initializes a new VAArgs source in place.
void vaargs_init(VAArgs *this, argcount_t arg_count, va_list args);

// ===============================================================
//  Array argument source
// ===============================================================

/**
 * Argument source for arguments in a SepArray.
 */
typedef struct ArrayArgs {
	struct ArgumentSource base;
	// the array
	SepArray *array;
	// iterator for going over the elements
	SepArrayIterator iterator;
	// a place for the argument that get_next_argument returns
	Argument current_arg;
} ArrayArgs;

// Initializes a new ArrayArgs source in place.
void arrayargs_init(ArrayArgs *this, SepArray *args);

// ===============================================================
//  Function call argument handling
// ===============================================================

// Sets up the all the call arguments inside the execution scope. Also validates them.
// Any problems found will be reported as an exception SepV in the return value.
SepV funcparam_pass_arguments(struct ExecutionFrame *frame, struct SepFunc *func, struct SepObj *scope, ArgumentSource *arguments);

/*****************************************************************/

#endif
