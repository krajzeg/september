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

/*****************************************************************/

#endif
