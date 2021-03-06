/*****************************************************************
 **
 ** io/decoder.c
 **
 ** Implementation for decoder.h - decoding binary module files.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "../common/debugging.h"

#include "../vm/opcodes.h"
#include "../vm/functions.h"
#include "../vm/support.h"
#include "loader.h"
#include "decoder.h"

// ===============================================================
//  Module file format constants
// ===============================================================

enum OpcodeFlags {
	MFILE_FLAG_LOCALS = 0x80,
	MFILE_FLAG_FETCH_PROPERTY = 0x40,
	MFILE_FLAG_CREATE_PROPERTY = 0x20,
	MFILE_FLAG_STORE = 0x10,
	MFILE_FLAG_POP = 0x08
};

enum ParamFlags {
	MFILE_P_LAZY_EVALUATED = 0x01,
	MFILE_P_SINK = 0x10,
	MFILE_P_NAMED_SINK = 0x20,
	MFILE_P_OPTIONAL = 0x80
};

// ===============================================================
//  Internal constants
// ===============================================================

// One byte type-tags used for constants in SEPT files.
enum ConstantType {
	CT_INT = 1,
	CT_STRING = 2
};

// ===============================================================
//  Decoding
// ===============================================================

uint8_t decoder_read_byte(BytecodeDecoder *this, SepV *error) {
	return this->source->vt->get_next_byte(this->source, error);
}

int32_t decoder_read_int(BytecodeDecoder *this, SepV *error) {
	SepV err = SEPV_NOTHING;

	uint8_t first_byte = decoder_read_byte(this, &err) or_fail_with(0);
	bool negative = (first_byte & 0x80);
	int32_t magnitude = 0;
	if ((first_byte & 0x60) == 0x60) {
		// long form format:
		//   {number of bytes}{byte1}{byte2}...{byteN}
		//                    ----- little endian ----
		uint8_t bytes_to_read = first_byte & 0x1F;
		while (bytes_to_read--) {
			uint8_t next_byte = decoder_read_byte(this, &err) or_fail_with(0);
			magnitude = (magnitude << 8) | next_byte;
		}
	} else if (first_byte & 0x40) {
		// two-byte format: (S10xxxxx)(xxxxxxxx)
		uint8_t second_byte = decoder_read_byte(this, &err) or_fail_with(0);
		magnitude = ((first_byte & 0x3F) << 8) | second_byte;
	} else {
		// simple format: (S0xxxxxx)
		magnitude = first_byte & 0x3F;
	}

	// take sign into account and return
	return negative ? -magnitude : magnitude;
}

char *decoder_read_string(BytecodeDecoder *this, SepV *error) {
	SepV err = SEPV_NOTHING;
	int32_t length = decoder_read_int(this, &err)
		or_fail_with(NULL);
	if (length < 0)
		fail(NULL, exception(exc.EMalformedModule, "Negative string constant length encountered."));

	char *string = mem_unmanaged_allocate(length + 1);
	string[length] = '\0';

	char *c = string;
	while(length--) {
		(*c++) = decoder_read_byte(this, &err)
			or_go(clean_up);
	}
	return string;

clean_up:
	mem_unmanaged_free(string);
	fail(NULL, err);
}

BytecodeDecoder *decoder_create(ByteSource *source) {
	BytecodeDecoder *decoder = mem_unmanaged_allocate(sizeof(BytecodeDecoder));
	decoder->source = source;
	return decoder;
}

void decoder_verify_header(BytecodeDecoder *this, SepV *error) {
	static char EXPECTED_HEADER[] = "SEPT";

	SepV err = SEPV_NOTHING;

	// read enough bytes to verify the header and compare
	// with expected - any mismatch ends up with
	// ENotSeptemberFile being raised
	char *c = EXPECTED_HEADER;
	while(*c) {
		uint8_t byte = decoder_read_byte(this, &err);
			or_fail();
		if (byte != *c)
			fail(exception(exc.EMalformedModule, "The file does not seem to be a September module file."));

		c++;
	}
}

ConstantPool *decoder_read_cpool(BytecodeDecoder *this, SepV *error) {
	SepV err = SEPV_NOTHING;

	// read the number of constants and create a pool
	int32_t num_constants = decoder_read_int(this, &err);
		or_fail_with(NULL);
	log("decoder", "Reading a pool of %d constants.", num_constants);
	ConstantPool * pool = cpool_create(num_constants);

	// fill the pool
	int index;
	for (index = 0; index < num_constants; index++) {
		uint8_t constant_type = decoder_read_byte(this, &err);
			or_fail_with(pool);
		switch(constant_type) {
			case CT_INT:
			{
				SepInt integer = decoder_read_int(this, &err);
					or_fail_with(pool);
				cpool_add_int(pool, integer);
				break;
			}

			case CT_STRING:
			{
				char *string = decoder_read_string(this, &err);
					or_fail_with(pool);
				cpool_add_string(pool, string);
				log("decoder", "constant %d: %s", index+1, string);
				break;
			}

			default:
				fail(pool, exception(exc.EMalformedModule, "Unrecognized constant type tag: %d.", constant_type));
		}
	}

	// return the filled pool
	return pool;
}

void decoder_read_block_params(BytecodeDecoder *this, BlockPool *pool, int param_count, SepV *error) {
	SepV err = SEPV_NOTHING;
	// initialize the block with the right amount of space
	CodeBlock *block = bpool_start_block(pool, param_count);

	// read parameter list
	int index;
	for (index = 0; index < param_count; index++) {
		FuncParam parameter;

		// deal with the flags first
		uint8_t param_flags = decoder_read_byte(this, &err);
			or_fail();
		parameter.flags.lazy = (param_flags & MFILE_P_LAZY_EVALUATED) != 0;
		parameter.flags.optional = (param_flags & MFILE_P_OPTIONAL) != 0;

		parameter.flags.type = PT_STANDARD_PARAMETER;
		if ((param_flags & MFILE_P_SINK) != 0)
			parameter.flags.type = PT_POSITIONAL_SINK;
		if ((param_flags & MFILE_P_NAMED_SINK) != 0)
			parameter.flags.type = PT_NAMED_SINK;

		// default value?
		if (parameter.flags.optional) {
			// read default value reference
			parameter.default_value_reference = decoder_read_int(this, &err);
				or_fail();
		}

		// read the parameter name
		// it's important to use sepstr_for to make sure the string will be interned
		// the string cache might end up being the only object with a reference to it
		// that prevents it from being GC'd
		parameter.name = sepstr_for(decoder_read_string(this, &err));
			or_fail();

		// remember the parameter
		block->parameters[index] = parameter;
	}
}

void decoder_read_block_code(BytecodeDecoder *this, BlockPool *pool, SepV *error) {
	SepV err = SEPV_NOTHING;
	
	uint8_t opcode = decoder_read_byte(this, &err);
		or_fail();
	
	// read operations until end of block  
	while (opcode != 0xFF) {
		// handle pre-operation flags
		if (opcode & MFILE_FLAG_LOCALS)
			bpool_write_code(pool, OP_PUSH_LOCALS);
		if (opcode & MFILE_FLAG_FETCH_PROPERTY) {
			// write the operation and its argument
			bpool_write_code(pool, OP_FETCH_PROPERTY);
			bpool_write_code(pool, (int16_t)decoder_read_int(this, &err));
		}
		if (opcode & MFILE_FLAG_CREATE_PROPERTY) {
			// write the operation and its argument
			bpool_write_code(pool, OP_CREATE_PROPERTY);
			bpool_write_code(pool, (int16_t)decoder_read_int(this, &err));
		}
		
		// operation (module file values match opcode enum, so this is 1:1)
		// nops are skipped altogether
		uint8_t raw_op = opcode & 0x7;
		if (raw_op)
			bpool_write_code(pool, raw_op);
		
		// read arguments depending on opcode
		uint8_t op_arg_count, arg_index;
		switch(raw_op) {
			case OP_PUSH_CONST:
				// constant
				bpool_write_code(pool, (int16_t)decoder_read_int(this, &err));
					or_fail();
				break;
			case OP_LAZY_CALL:
				// count arguments
				op_arg_count = decoder_read_byte(this, &err);
					or_fail();
				bpool_write_code(pool, op_arg_count);
				// read them all
				for (arg_index = 0; arg_index < op_arg_count; arg_index++) {
					bpool_write_code(pool, (int16_t)decoder_read_int(this, &err));
						or_fail();
				}
				break;
				
		}
		
		// handle post-operation flags
		if (opcode & MFILE_FLAG_STORE) {
			bpool_write_code(pool, OP_STORE);
		}
		if (opcode & MFILE_FLAG_POP)
			bpool_write_code(pool, OP_POP);
		
		// next!
		opcode = decoder_read_byte(this, &err);
			or_fail();
	}
	
	// close the code block
	bpool_end_block(pool);
}

BlockPool *decoder_read_bpool(BytecodeDecoder *this, SepModule *module, SepV *error) {
	SepV err = SEPV_NOTHING;
	
	BlockPool *blocks = bpool_create(module, 2048);
	
	// read functions until we run out 
	uint8_t function_header_byte = decoder_read_byte(this, &err);
		or_fail_with(blocks);
		
	// read until the end marker
	while (function_header_byte != 0xFF) {
		// if it's not 0xFF, it's the next function's parameter count
		uint8_t parameter_count = function_header_byte;

		// read the function
		decoder_read_block_params(this, blocks, parameter_count, &err);
			or_fail_with(blocks);
		decoder_read_block_code(this, blocks, &err);
			or_fail_with(blocks);
		
		// next!
		function_header_byte = decoder_read_byte(this, &err);
			or_fail_with(blocks);
	}
	
	// seal the block to make it usable
	bpool_seal(blocks);
	
	return blocks;
}

void decoder_read_pools(BytecodeDecoder *this, SepModule *module, SepV *error) {
	SepV err = SEPV_NOTHING;

	decoder_verify_header(this, &err);
		or_fail();
		
	module->constants = decoder_read_cpool(this, &err);
		or_fail();
	module->blocks = decoder_read_bpool(this, module, &err);
		or_fail();
}

void decoder_free(BytecodeDecoder *this) {
	if (!this) return;

	if (this->source)
		this->source->vt->close_and_free(this->source);
	mem_unmanaged_free(this);
}
