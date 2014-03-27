#ifndef _SEP_VM_H
#define _SEP_VM_H

/*****************************************************************
 **
 ** runtime/vm.h
 **
 ** Virtual machine structures. 
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "types.h"
#include "functions.h"
#include "objects.h"
#include "arrays.h"
#include "stack.h"
#include "module.h"

// ===============================================================
//  Pre-defining structs
// ===============================================================

struct SepVM;
struct SepArray;
struct ExecutionFrame;

// ===============================================================
//  Argument sources
// ===============================================================

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
	// gets the total number of arguments to pass
	argcount_t (*argument_count)(ArgumentSource *);
	// returns an argument and advances the source to the next one
	SepV (*get_next_argument)(ArgumentSource *);
} ArgumentSourceVT;


/**
 * Argument source for getting arguments stored in bytecode.
 */
typedef struct BytecodeArgs {
	struct ArgumentSource base;
	// source for arguments
	struct ExecutionFrame *source_frame;
	// stored argument count
	argcount_t argument_count;
} BytecodeArgs;

// Initializes a new bytecode source in place.
void bytecodeargs_init(BytecodeArgs *this, struct ExecutionFrame *frame);

/**
 * Argument source for getting arguments from a C vararg function.
 */
typedef struct VAArgs {
	struct ArgumentSource base;
	// stored argument count
	argcount_t argument_count;
	// a started va_list with the arguments as SepVs
	va_list c_arg_list;
} VAArgs;

// Initializes a new VAArgs source in place.
void vaargs_init(VAArgs *this, argcount_t arg_count, va_list args);

/**
 * Argument source for arguments in a SepArray.
 */
typedef struct ArrayArgs {
	struct ArgumentSource base;
	// the array
	SepArray *array;
	// iterator for going over the elements
	SepArrayIterator iterator;
} ArrayArgs;

// Initializes a new ArrayArgs source in place.
void arrayargs_init(ArrayArgs *this, SepArray *args);

// ===============================================================
//  Execution frame
// ===============================================================

typedef struct ExecutionFrame {
	// the VM responsible for this frame
	struct SepVM *vm;
	// the module this frame comes from
	SepModule *module;
	// function for this frame
	SepFunc *function;
	// instruction pointer
	CodeUnit *instruction_ptr;
	// the data stack accessed by VM operations
	SepStack *data;
	// execution scope
	SepV locals;
	// return value from this frame
	SepItem return_value;
	// execution finished?
	bool finished;
	// execution called into another frame?
	bool called_another_frame;

	// Those pointers are set up by the VM to make up a linked
	// list of frames.
	struct ExecutionFrame *next_frame;
	struct ExecutionFrame *prev_frame;
} ExecutionFrame;

// Execute instructions from this frame, up to a given limit.
int frame_execute_instructions(ExecutionFrame *frame, int limit);

// Reads the next code unit from this frame.
CodeUnit frame_read(ExecutionFrame *frame);
// Read a constant from this frame's module.
SepV frame_constant(ExecutionFrame *frame, uint32_t index);
// Get a code block from this frame's module.
CodeBlock *frame_block(ExecutionFrame *frame, uint32_t index);

// Finalize this frame with a given return value.
void frame_return(ExecutionFrame *frame, SepItem return_value);
// Raise an exception inside a frame and finalizes it.
void frame_raise(ExecutionFrame *frame, SepV exception);

// ===============================================================
//  The virtual machine
// ===============================================================

typedef struct SepVM {
	// the "builtins" object which is available in all execution
	// scopes
	SepObj *syntax;
	// the data stack shared by all execution frames
	SepStack *data;
	// the whole execution stack, with the main function of the main
	// module at the bottom, and the current function at the top
	ExecutionFrame frames[1024];
	// how deep the frame currently executed is
	int frame_depth;
} SepVM;

// Creates a new VM, starting with a specified function.
SepVM *vm_create(SepModule *module, SepObj *globals);
// Runs the VM until it halts, either by finishing execution normally
// or terminating with an exception. Either way, whatever is the final
// result is returned.
SepV vm_run(SepVM *this);
// Initializes the root execution frame based on a module.
void vm_initialize_root_frame(SepVM *this, SepModule *module);

// Initializes a scope object for execution of a given function. This sets up
// the prototype chain for the scope to include the 'syntax' object, the 'this'
// pointer, and the declaration scope of the function. It also sets up the
// 'locals' and 'this' properties.
void vm_initialize_scope(SepVM *this, SepFunc *func, SepObj *exec_scope, ExecutionFrame *exec_frame);
// Initializes an execution frame for running a given function.
// It takes as arguments the frame to initialize, the function to run within the frame,
// and the execution scope object. The scope object should have the proper prototype
// chain already set up, using vm_initialize_scope() or other means.
void vm_initialize_frame(SepVM *this, ExecutionFrame *frame, SepFunc *func, SepV scope);
// Destroys a VM.
void vm_free(SepVM *this);

// ===============================================================
// Invoking function calls from C code
// ===============================================================

// Makes a subcall from within a built-in function. The result of the subcall is then returned.
// Any number of arguments can be passed in, and they should be passed as SepVs.
SepItem vm_invoke(SepVM *this, SepV callable, uint8_t argument_count, ...);
// Makes a subcall, but runs the callable in a specified scope (instead of using a normal
// this + declaration scope + locals scope). Any arguments passed will be added to the
// scope you specify.
SepItem vm_invoke_in_scope(SepVM *this, SepV callable, SepV execution_scope, uint8_t argument_count, ...);
// Makes a subcall from within a built-in function. The result of the subcall is then returned.
// An argument source with the arguments of this call has to be provided.
SepItem vm_invoke_with_argsource(SepVM *this, SepV callable, SepV custom_scope, ArgumentSource *args);

// ===============================================================
// Resolving lazy values
// ===============================================================

// Uses the VM to resolve a lazy value.
SepV vm_resolve(SepVM *this, SepV lazy_value);
// Uses the VM to resolve a lazy value in a specified scope (instead
// of in its parent scope). Used together with SEPV_LITERALS allows
// resolving expressions like 'name' to the string "name".
SepV vm_resolve_in(SepVM *this, SepV lazy_value, SepV scope);
// Uses the VM to resolve a lazy value to the identifier that it names.
// This uses a special "Literals" scope in which every identifier resolves
// to itself.
SepV vm_resolve_as_literal(SepVM *this, SepV lazy_value);

/*****************************************************************/

#endif
