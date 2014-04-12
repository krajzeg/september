#ifndef _SEP_OPCODES_H
#define _SEP_OPCODES_H

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
 * Each code unit is either an encoded instruction to execute,
 * or the argument of such an instruction.
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
//  References
// ===============================================================

/**
 * In September bytecode, opcode arguments are stored as references
 * into either the constant pool or the function pool. Those references
 * are simple integers that have to be decoded.
 */

/**
 * All the possible reference types that can be stored in bytecode.
 */
typedef enum PoolReferenceType {
	PRT_CONSTANT = -1,
	PRT_FUNCTION = 0
} PoolReferenceType;

// Given a reference from bytecode, returns its type.
PoolReferenceType decode_reference_type(CodeUnit reference);
// Given a reference from bytecode, returns the index inside the pool that
// it is referring to.
uint32_t decode_reference_index(CodeUnit reference);

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
