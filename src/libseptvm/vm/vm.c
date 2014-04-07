// ===============================================================
//  Includes
// ===============================================================

#include <stdio.h>
#include <stdlib.h>
#include "mem.h"
#include "gc.h"
#include "objects.h"
#include "exceptions.h"
#include "arrays.h"
#include "opcodes.h"
#include "vm.h"

#include "../common/debugging.h"
#include "../vm/runtime.h"
#include "../vm/support.h"

// ===============================================================
//  Global variables
// ===============================================================

// All the globals
LibSeptVMGlobals lsvm_globals = {NULL};

// While a VM is running, a pointer to it is always stored here. Every thread can
// only run one VM at a time, and each thread has its own VM.
__thread SepVM *_currently_running_vm = NULL;

// ===============================================================
//  Execution frame
// ===============================================================

// Execute instructions from this frame, up to a given limit.
int frame_execute_instructions(ExecutionFrame *frame, int limit) {
	SepFunc *function = frame->function;
	return function->vt->execute_instructions(function, frame, limit);
}

// Reads the next code unit from this frame.
CodeUnit frame_read(ExecutionFrame *frame) {
	CodeUnit code = *(frame->instruction_ptr++);
	return code;
}

// Read a constant from this frame's module.
SepV frame_constant(ExecutionFrame *frame, uint32_t index) {
	return cpool_constant(frame->module->constants, index);
}

CodeBlock *frame_block(ExecutionFrame *frame, uint32_t index) {
	return bpool_block(frame->module->blocks, index);
}

// Finalize this frame with a given return value.
void frame_return(ExecutionFrame *frame, SepItem return_value) {
	frame->return_value = return_value;
	frame->finished = true;
}

// Raise an exception inside a frame and finalizes it.
void frame_raise(ExecutionFrame *frame, SepV exception) {
	frame->return_value = item_rvalue(exception);
	frame->finished = true;
}

// Registers a value as a GC root to prevent it from being freed before its unused.
void frame_register(ExecutionFrame *frame, SepV value) {
	if (sepv_is_pointer(value))
		ga_push(&frame->gc_roots, &value);
}

// Releases a value previously held in the frame's GC root table,
// making it eligible for garbage collection again.
void frame_release(ExecutionFrame *frame, SepV value) {
	GenericArray *roots = &frame->gc_roots;
	GenericArrayIterator it = ga_iterate_over(roots);
	while (!gait_end(&it)) {
		SepV itv = *((SepV*)gait_current(&it));

		if (itv == value) {
			// remove the element
			uint32_t index = gait_index(&it);
			SepV value_from_end = *((SepV*)ga_pop(roots));
			if (index < ga_length(roots))
				ga_set(roots, index, &value_from_end);
		}

		// next element
		gait_advance(&it);
	}
}

// ===============================================================
//  Bytecode argument source
// ===============================================================

argcount_t _bytecode_argument_count(ArgumentSource *_this) {
	return ((BytecodeArgs*)_this)->argument_count;
}

SepV _bytecode_next_argument(ArgumentSource *_this) {
	ExecutionFrame *frame = ((BytecodeArgs*)_this)->source_frame;

	// read the argument code
	CodeUnit argument_code = frame_read(frame);

	// did we get a constant or a block?
	if (argument_code > 0) {
		// constant, that's easy!
		return frame_constant(frame, argument_code);
	} else {
		// block - that's a lazy evaluated argument
		CodeBlock *block = frame_block(frame, -argument_code);
		if (!block) {
			return sepv_exception(exc.EInternal,
					sepstr_sprintf("Code block %d out of bounds.", -argument_code));
		}
		SepFunc *argument_l = (SepFunc*)lazy_create(block, frame->locals);
		return func_to_sepv(argument_l);
	}
}

ArgumentSourceVT bytecode_args_vt = {
	&_bytecode_argument_count,
	&_bytecode_next_argument
};

// Initializes a new bytecode source in place.
void bytecodeargs_init(BytecodeArgs *this, ExecutionFrame *frame) {
	this->base.vt = &bytecode_args_vt;
	this->source_frame = frame;
	this->argument_count = (argcount_t)frame_read(frame);
}

// ===============================================================
//  va_list argument source
// ===============================================================

argcount_t _va_argument_count(ArgumentSource *_this) {
	return ((VAArgs*)_this)->argument_count;
}

SepV _va_next_argument(ArgumentSource *_this) {
	return va_arg(((VAArgs*)_this)->c_arg_list, SepV);
}

ArgumentSourceVT va_args_vt = {
	&_va_argument_count,
	&_va_next_argument
};

// Initializes a new VAArgs source in place.
void vaargs_init(VAArgs *this, argcount_t arg_count, va_list args) {
	this->base.vt = &va_args_vt;
	this->argument_count = arg_count;
	va_copy(this->c_arg_list, args);
}

// ===============================================================
//  Array-based argument source
// ===============================================================

argcount_t _arrayargs_argument_count(ArgumentSource *_this) {
	return array_length(((ArrayArgs*)_this)->array);
}

SepV _arrayargs_next_argument(ArgumentSource *_this) {
	ArrayArgs *this = (ArrayArgs*)_this;
	return arrayit_next(&this->iterator);
}

ArgumentSourceVT array_args_vt = {
	&_arrayargs_argument_count,
	&_arrayargs_next_argument
};

// Initializes a new VAArgs source in place.
void arrayargs_init(ArrayArgs *this, SepArray *array) {
	this->base.vt = &array_args_vt;
	this->array = array;
	this->iterator = array_iterate_over(array);
}

// ===============================================================
//  The virtual machine
// ===============================================================

SepVM *vm_create(SepModule *module, SepObj *syntax) {
	SepVM *vm = mem_unmanaged_allocate(sizeof(SepVM));

	// initialize all execution frames for safe usage later
	int f;
	for (f = 0; f < VM_FRAME_COUNT; f++) {
		ExecutionFrame *frame = &vm->frames[f];
		ga_init(&frame->gc_roots, 4, sizeof(SepV), &allocator_unmanaged);
	}

	// create the data stack
	vm->data = stack_create();
	vm->syntax = syntax;

	// start a frame for the root function and set it to be current
	vm_initialize_root_frame(vm, module);
	vm->frame_depth = 0;

	// return
	return vm;
}

SepV vm_run(SepVM *this) {
	// sanity check - one VM allowed per thread
	SepVM *previously_running = _currently_running_vm;
	if (previously_running && previously_running != this) {
		return sepv_exception(exc.EInternal, sepstr_for("An attempt was made to run a second VM in one thread."));
	} else {
		_currently_running_vm = this;
	}

	// store the starting depth for this run - once we leave this level,
	// we stop execution and return the last return value
	int starting_depth = this->frame_depth;

	// mark our place on the stack so that we can unwind it properly
	// when exception is thrown
	stack_push_rvalue(this->data, SEPV_UNWIND_MARKER);

	// repeat until we have a return value ready
	// (a break handles this from inside the body)
	ExecutionFrame *current_frame;
	while (true) {
		// grab a frame to execute
		current_frame = &this->frames[this->frame_depth];

		// execute instructions (if we're not done)
		if ((!current_frame->finished) && (!current_frame->called_another_frame))
			frame_execute_instructions(current_frame, 1000);

		// did this frame call into another one?
		if (current_frame->called_another_frame) {
			// clear the flag
			current_frame->called_another_frame = false;
			// move down the stack
			// the call operation itself has already filled in the next frame
			this->frame_depth++;
			// put an unwind marker on the stack
			stack_push_rvalue(this->data, SEPV_UNWIND_MARKER);

			log("vm", "(%d) New execution frame created (interpreted call).", this->frame_depth);
		}

		// did we finish the frame?
		if (current_frame->finished) {
			// with an exception?
			if (sepv_is_exception(current_frame->return_value.value)) {
				log("vm", "(%d) Execution frame finished with exception.", this->frame_depth);
				log("vm", "Unwinding to level (%d).", starting_depth);

				int frames_to_unwind = this->frame_depth - starting_depth + 1;

				// drop all the frames from this VM run
				this->frame_depth = starting_depth - 1;

				// clear data from the stack
				while (frames_to_unwind > 0) {
					SepV stack_value = stack_pop_value(this->data);
					if (stack_value == SEPV_UNWIND_MARKER)
						frames_to_unwind--;
				}

				// and pass the exception to the authorities
				goto cleanup_and_return;
			}

			// nope, just a normal return
			log("vm", "(%d) Execution frame finished normally.", this->frame_depth);

			// pop the frame
			this->frame_depth--;
			// unwind the stack (usually it will be just the unwind marker, but in
			// case of a 'break' or 'return' it might be more items)
			SepV stack_value = stack_pop_value(this->data);
			while (stack_value != SEPV_UNWIND_MARKER)
				stack_value = stack_pop_value(this->data);

			// push the return value on the data stack for its parent
			if (this->frame_depth >= starting_depth) {
				ExecutionFrame *parent_frame = &this->frames[this->frame_depth];
				stack_push_item(parent_frame->data, current_frame->return_value);
			} else {
				// this is the end of this execution
				// return the result of the whole execution
				goto cleanup_and_return;
			}
		}
	}

cleanup_and_return:
	_currently_running_vm = previously_running;
	return current_frame->return_value.value;
}

// Initializes the root execution frame based on a module.
void vm_initialize_root_frame(SepVM *this, SepModule *module) {
	// root function always starts at the 0th frame
	ExecutionFrame *frame = &this->frames[0];

	// setup the frame
	frame->vm = this;
	frame->data = this->data;
	frame->return_value = item_rvalue(SEPV_NOTHING);
	frame->finished = false;
	frame->called_another_frame = false;
	frame->module = module;
	frame->instruction_ptr = NULL;
	frame->prev_frame = NULL;
	frame->next_frame = &this->frames[1];

	// set it as the execution scope for the root function
	frame->locals = obj_to_sepv(module->root);

	// give it the proper properties
	props_add_field(module->root, "locals", frame->locals);
	props_add_field(module->root, "this", frame->locals);

	// find the root function itself
	CodeBlock *root_block = bpool_block(module->blocks, 1);
	InterpretedFunc *root_func = ifunc_create(root_block, frame->locals);
	frame->function = (SepFunc*)root_func;

	// empty the additional GC root table
	ga_clear(&frame->gc_roots);

	// perform function specific initialization for the root function
	root_func->base.vt->initialize_frame((SepFunc*)root_func, frame);
}

void vm_initialize_scope(SepVM *this, SepFunc *func, SepObj* exec_scope, ExecutionFrame *exec_frame) {
	SepV exec_scope_v = obj_to_sepv(exec_scope);

	// take prototypes based on the function
	SepV this_ptr_v = func->vt->get_this_pointer(func);
	SepV decl_scope_v = func->vt->get_declaration_scope(func);
	SepArray *prototypes = array_create(4);
	array_push(prototypes, obj_to_sepv(this->syntax));
	if ((this_ptr_v != SEPV_NOTHING) && (this_ptr_v != exec_scope_v))
		array_push(prototypes, this_ptr_v);
	if ((decl_scope_v != SEPV_NOTHING) && (decl_scope_v != exec_scope_v))
		array_push(prototypes, decl_scope_v);
	array_push(prototypes, obj_to_sepv(rt.Object));

	// set the prototypes property on the local scope
	exec_scope->prototypes = obj_to_sepv(prototypes);

	// enrich the locals object with properties pointing to important objects
	props_add_field(exec_scope, "locals", exec_scope_v);
	if (this_ptr_v != SEPV_NOTHING)
		props_add_field(exec_scope, "this", this_ptr_v);

	// add a 'return' function to the scope
	BuiltInFunc *return_f = make_return_func(exec_frame);
	obj_add_field(exec_scope, "return", func_to_sepv(return_f));
}

// Initializes an execution frame for running a given function.
void vm_initialize_frame(SepVM *this, ExecutionFrame *frame, SepFunc *func, SepV locals) {
	// setup the frame
	frame->vm = this;
	frame->function = func;
	frame->data = this->data;
	frame->return_value = item_rvalue(SEPV_NOTHING);
	frame->finished = false;
	frame->called_another_frame = false;

	// set an execution scope
	frame->locals = locals;

	// frames are contiguous in memory, so the next frame is right after
	frame->prev_frame = frame - 1;
	frame->next_frame = frame + 1;

	// we don't know the right values, but the function will initialize
	// those fields on its own if it cares about them
	frame->instruction_ptr = NULL;
	frame->module = NULL;

	// empty the additional GC root table
	ga_clear(&frame->gc_roots);

	// perform function specific initialization
	func->vt->initialize_frame(func, frame);
}

void vm_free(SepVM *this) {
	if (!this) return;
	mem_unmanaged_free(this);
}

// ===============================================================
//  Global access to VM instances
// ===============================================================

// Low-level implementation function for access to the thread-local.
SepVM *_vm_for_current_thread() {
	return _currently_running_vm;
}

// Returns the VM currently used running in this thread. Only one SepVM instance is
// allowed per thread.
SepVM *vm_current() {
	return lsvm_globals.vm_for_current_thread_func();
}

// Returns the current execution frame in the current thread.
ExecutionFrame *vm_current_frame() {
	SepVM *current = lsvm_globals.vm_for_current_thread_func();
	if (!current)
		return NULL;
	return &current->frames[current->frame_depth];
}

// ===============================================================
//  Garbage collection
// ===============================================================

// Queues all the objects directly reachable from this VM. These are objects on the
// data stack and all the execution frame scopes.
void vm_queue_gc_roots(GarbageCollection *gc) {
	// just one VM for now - this will have to change when concurrency support is added
	SepVM *vm = vm_current();
	if (!vm)
		return;

	// queue all items from the data stack
	GenericArrayIterator sit = ga_iterate_over(&vm->data->array);
	while (!gait_end(&sit)) {
		SepItem stack_item = *((SepItem*)gait_current(&sit));
		gc_add_to_queue(gc, stack_item.value);
		gait_advance(&sit);
	}

	// queue everything accessible from the execution frames
	int f = 0;
	for (; f <= vm->frame_depth; f++) {
		ExecutionFrame *frame = &vm->frames[f];
		gc_add_to_queue(gc, func_to_sepv(frame->function));
		gc_add_to_queue(gc, frame->locals);
		gc_add_to_queue(gc, frame->return_value.value);

		// additional GC roots - objects allocated by this frame
		GenericArrayIterator rit = ga_iterate_over(&frame->gc_roots);
		while (!gait_end(&rit)) {
			SepV value = *((SepV*)gait_current(&rit));
			gc_add_to_queue(gc, value);
			gait_advance(&rit);
		}
	}
}

// ===============================================================
//  Subcalls and lazy argument resolution
// ===============================================================

// Makes a subcall from within a built-in function. The result of the subcall is then returned.
// Any number of parameters can be passed in, and they should be passed as SepVs.
SepItem vm_invoke(SepVM *this, SepV callable, uint8_t argument_count, ...) {
	va_list args;
	va_start(args, argument_count);

	VAArgs source;
	vaargs_init(&source, argument_count, args);

	SepItem result = vm_invoke_with_argsource(this, callable, SEPV_NO_VALUE, (ArgumentSource*)(&source));
	va_end(args);
	return result;
}

// Makes a subcall, but runs the callable in a specified scope (instead of using a normal
// this + declaration scope + locals scope). Any arguments passed will be added to the
// scope you specify.
SepItem vm_invoke_in_scope(SepVM *this, SepV callable, SepV execution_scope, uint8_t argument_count, ...) {
	va_list args;
	va_start(args, argument_count);

	VAArgs source;
	vaargs_init(&source, argument_count, args);

	SepItem result = vm_invoke_with_argsource(this, callable, execution_scope, (ArgumentSource*)(&source));
	va_end(args);
	return result;
}

// Makes a subcall from within a built-in function. The result of the subcall is then returned.
// A started va_list of SepVs should be passed in by another vararg function.
SepItem vm_invoke_with_argsource(SepVM *this, SepV callable, SepV custom_scope, ArgumentSource *args) {
	// get a call target
	SepFunc *func = sepv_call_target(callable);
	if (!func) {
		return si_exception(exc.EWrongType,
			sepstr_for("Attempted to call an object which is not callable."));
	}

	// create the execution scope
	bool custom_scope_provided = custom_scope != SEPV_NO_VALUE;
	SepObj *scope = custom_scope_provided ? sepv_to_obj(custom_scope) : obj_create();
	ExecutionFrame *calling_frame = &this->frames[this->frame_depth];
	SepV argument_exc = funcparam_pass_arguments(calling_frame, func, scope, args);
		or_propagate(argument_exc);

	// initialize an execution frame
	log("vm", "(%d) New execution frame created (subcall from b-in).", this->frame_depth+1);
	ExecutionFrame *frame = &this->frames[this->frame_depth+1];

	if (!custom_scope_provided)
		vm_initialize_scope(this, func, scope, frame);
	vm_initialize_frame(this, frame, func, obj_to_sepv(scope));
	this->frame_depth++;

	// run to get result
	SepV return_value = vm_run(this);

	// register the return value in the parent frame to prevent it from being GC'd
	gc_register(return_value);

	return item_rvalue(return_value);
}

// Uses the VM to resolve a lazy value.
SepV vm_resolve(SepVM *this, SepV lazy_value) {
	if (sepv_is_lazy(lazy_value)) {
		// function - resolve it in it's default scope
		SepFunc *func = sepv_to_func(lazy_value);
		SepV scope = func->vt->get_declaration_scope(func);
		return vm_resolve_in(this, lazy_value, scope);
	} else {
		// not a function, no resolution needed
		return lazy_value;
	}
}

// Uses the VM to resolve a lazy value in a specified scope (instead
// of in its parent scope).
SepV vm_resolve_in(SepVM *this, SepV lazy_value, SepV scope) {
	// maybe it doesn't need resolving?
	if (!sepv_is_lazy(lazy_value))
		return lazy_value;

	// it does - extract the function and set up a frame for it
	log("vm", "(%d) New execution frame created (value resolve).", this->frame_depth+1);

	SepFunc *func = sepv_to_func(lazy_value);
	ExecutionFrame *frame = &this->frames[this->frame_depth+1];
	vm_initialize_frame(this, frame, func, scope);
	this->frame_depth++;

	// run from that point until 'func' returns
	SepV return_value = vm_run(this);

	// register the return value in the parent frame to prevent it from being GC'd
	gc_register(return_value);

	// return its return value
	return return_value;
}

// Uses the VM to resolve a lazy value to the identifier that it names.
// This uses a special "Literals" scope in which every identifier resolves
// to itself.
SepV vm_resolve_as_literal(SepVM *this, SepV lazy_value) {
	return vm_resolve_in(this, lazy_value, SEPV_LITERALS);
}

// ===============================================================
//  Master/slave configuration of the library
// ===============================================================

// Initializes a slave libseptvm (as used inside a module DLL/.so). This is needed
// so that things like the memory manager can be shared with the master process.
void libseptvm_initialize_slave(LibSeptVMGlobals *parent_config) {
	lsvm_globals = *parent_config;
}

// Initializes the master libseptvm in the interpreter.
void libseptvm_initialize() {
	lsvm_globals.vm_for_current_thread_func = &_vm_for_current_thread;
	lsvm_globals.memory = mem_initialize(0x10000);
	lsvm_globals.debugged_module_names = mem_unmanaged_allocate(4096);
}
