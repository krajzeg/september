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
#include "code.h"
#include "functions.h"
#include "objects.h"
#include "stack.h"
#include "module.h"

// ===============================================================
//  Pre-defining structs
// ===============================================================

struct SepVM;
struct ExecutionFrame;

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
void vm_initialize_scope(SepVM *this, SepFunc *func, SepObj *locals_obj);
// Initializes an execution frame for running a given function.
// It takes as arguments the frame to initialize, the function to run within the frame,
// and the execution scope object. The scope object should have the proper prototype
// chain already set up, using vm_initialize_scope() or other means.
void vm_initialize_frame(SepVM *this, ExecutionFrame *frame, SepFunc *func, SepV scope);
// Destroys a VM.
void vm_free(SepVM *this);

// ===============================================================
//  Subcalls and lazy argument resolution
// ===============================================================

// Makes a subcall from within a built-in function. The result of the subcall is then returned.
// Any number of parameters can be passed in, and they should be passed as SepVs.
SepItem vm_subcall(SepVM *this, SepFunc *func, uint8_t parameter_count, ...);

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
