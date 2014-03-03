#ifndef _SEP_CODE_H
#define _SEP_CODE_H

/*****************************************************************
 **
 ** vm/code.h
 **
 ** Lowest level VM structures, going down to single instructions
 ** and below. Defines everything from VM opcodes and their
 ** behavior, all the way up to reusable code blocks.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdint.h>

// ===============================================================
//  Pre-declared structs
// ===============================================================

struct ExecutionFrame;
struct SepModule;
struct FuncParam;

// ===============================================================
//  Basic types
// ===============================================================

/**
 * Instruction streams inside code blocks consist of code units.
 * Each code unit is either an ID of an instruction to execute,
 * or an argument of such an instruction.
 */
typedef int16_t CodeUnit;

/**
 * Every operation supported by the VM is implemented by mutating
 * the execution frame.
 *
 * Through the frame, instructions can modify the data stack,
 * finish the execution of the frame, change its return value or
 * push a completely new frame on the execution stack.
 *
 * They can also read additional arguments using the instruction
 * pointer.
 */
typedef void (*InstructionLogic)(struct ExecutionFrame *frame);

// ===============================================================
//  Instructions
// ===============================================================

/**
 * All the operation codes used.
 */
enum OpCodes {
	// actual operations
	OP_NOP = 0x0,
	OP_PUSH_CONST = 0x1,
	OP_LAZY_CALL = 0x4,

	// 'virtual' operations implementing the instruction flags
	OP_PUSH_LOCALS   = 0x8,
	OP_FETCH_PROPERTY    = 0x9,
	OP_POP           = 0xA,
	OP_STORE         = 0xB,
	OP_CREATE_PROPERTY  = 0xC,

	// maximum value
	OP_MAX
};

/**
 * Instruction logic lookup table for transforming opcodes
 * during execution.
 */
extern InstructionLogic instruction_lut[];

// ===============================================================
//  Code blocks
// ===============================================================

/**
 * Represents a single code block from a module file. Those are just
 * the instructions - to get something callable, the block is wrapped
 * in an InterpretedFunc. Many function instances can point to the same
 * block, allowing for function instances with the same logic, but
 * different scopes/properties (which is how closures will work).
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

/*****************************************************************/

#endif
