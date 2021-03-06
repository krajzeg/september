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
struct GarbageCollection;
struct ExecutionFrame;
struct ArgumentSource;

// ===============================================================
//  Constants
// ===============================================================

// the maximum call depth allowed - the number of execution frames per VM
#define VM_FRAME_COUNT 1024

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

	// an array of objects allocated in this frame
	// all objects allocated within a frame have to be kept until
	// the frame finishes execution, so they are added as GC roots
	// during garbage collection
	GenericArray gc_roots;

	// These pointers are set up by the VM to make up a linked
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

// Registers a value as a GC root to prevent it from being freed before its unused.
void frame_register(ExecutionFrame *frame, SepV value);
// Releases a value previously held in the frame's GC root table,
// making it eligible for garbage collection again.
void frame_release(ExecutionFrame *frame, SepV value);

// ===============================================================
//  The virtual machine
// ===============================================================

typedef struct SepVM {
	// the data stack shared by all execution frames
	SepStack *data;
	// the whole execution stack, with the main function of the main
	// module at the bottom, and the current function at the top
	ExecutionFrame frames[VM_FRAME_COUNT];
	// how deep the frame currently executed is
	int frame_depth;
} SepVM;

// Creates a new VM for running a specified September module.
SepVM *vm_create(SepModule *module);
// Runs the VM until it halts, either by finishing execution normally
// or terminating with an exception. Either way, whatever is the final
// result on the stack, is returned.
SepItem vm_run(SepVM *this);
// Initializes the root execution frame based on a module.
void vm_initialize_root_frame(SepVM *this, SepModule *module);

// Initializes a scope object for execution of a given function. This sets up
// the prototype chain for the scope to include the 'this'
// pointer, and the declaration scope of the function. It also sets up the
// 'locals' and 'this' properties.
void vm_initialize_scope(SepVM *this, SepFunc *func, SepObj *exec_scope, ExecutionFrame *exec_frame);
// Called instead of vm_initialize_scope when a custom scope object is to be used.
void vm_set_scope(ExecutionFrame *exec_frame, SepV custom_scope);
// Initializes an execution frame for running a given function.
// It takes as arguments the frame to initialize and the function to run within the frame.
// The execution scope should be set up separately afterwards using vm_initialize_scope/vm_set_scope.
void vm_initialize_frame(SepVM *this, ExecutionFrame *frame, SepFunc *func);
// Destroys a VM.
void vm_free(SepVM *this);

// ===============================================================
//  Global access to VM instances
// ===============================================================

// Returns the VM currently used running in this thread. Only one SepVM instance is
// allowed per thread.
SepVM *vm_current();
// Returns the current execution frame in the current thread.
ExecutionFrame *vm_current_frame();

// ===============================================================
//  Garbage collection
// ===============================================================

// Queues all the objects directly reachable from any of the currently running VMs for
// marking in the garbage collection process.
void vm_queue_gc_roots(struct GarbageCollection *gc);

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
SepItem vm_invoke_with_argsource(SepVM *this, SepV callable, SepV custom_scope, struct ArgumentSource *args);

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
