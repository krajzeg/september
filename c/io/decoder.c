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
#include "../common/errors.h"
#include "../vm/code.h"
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

// ===============================================================
//  Internal constants
// ===============================================================

// One byte type-tags used for constants in SEPT files.
enum ConstantType {
	CT_INT = 1,
	CT_STRING = 2
};

// ===============================================================
//  File sources
// ===============================================================

uint8_t filesource_get_byte(FileSource *this, SepError *out_err) {
	int byte = fgetc(this->file);
	if (byte == EOF)
		fail(0, e_unexpected_eof());
	return byte;
}

FileSource *filesource_open(const char *filename, SepError *out_err) {
	// try opening the file
	FILE *file = fopen(filename, "rb");
	if (file == NULL)
		fail(NULL, e_file_not_found(filename));

	// allocate and set up the data structure
	FileSource *source = malloc(sizeof(FileSource));
	source->base.get_next_byte = (GetNextByteFunc)&filesource_get_byte;
	source->base.free = (FreeDSFunc)&filesource_close;
	source->file = file;
	return source;
}

void filesource_close(FileSource *this) {
	if (!this) return;

	// close the file
	if (this->file)
		fclose(this->file);

	// free memory
	free(this);
}

// ===============================================================
//  Decoding
// ===============================================================

uint8_t decoder_read_byte(Decoder *this, SepError *out_err) {
	return this->source->get_next_byte(this->source, out_err);
}

int32_t decoder_read_int(Decoder *this, SepError *out_err) {
	SepError err = NO_ERROR;

	uint8_t first_byte = decoder_read_byte(this, &err) or_quit_with(0);
	bool negative = (first_byte & 0x80);
	int32_t magnitude = 0;
	if ((first_byte & 0x60) == 0x60) {
		// long form format:
		//   {number of bytes}{byte1}{byte2}...{byteN}
		//                    ----- little endian ----
		uint8_t bytes_to_read = first_byte & 0x1F;
		while (bytes_to_read--) {
			uint8_t next_byte = decoder_read_byte(this, &err) or_quit_with(0);
			magnitude = (magnitude << 8) | next_byte;
		}
	} else if (first_byte & 0x40) {
		// two-byte format: (S10xxxxx)(xxxxxxxx)
		uint8_t second_byte = decoder_read_byte(this, &err) or_quit_with(0);
		magnitude = ((first_byte & 0x3F) << 8) | second_byte;
	} else {
		// simple format: (S0xxxxxx)
		magnitude = first_byte & 0x3F;
	}

	// take sign into account and return
	return negative ? -magnitude : magnitude;
}

char *decoder_read_string(Decoder *this, SepError *out_err) {
	SepError err = NO_ERROR;
	int32_t length = decoder_read_int(this, &err)
		or_quit_with(NULL);
	if (length < 0)
		fail(NULL, e_malformed_module_file("Negative string length."));

	char *string = malloc(length + 1);
	string[length] = '\0';

	char *c = string;
	while(length--) {
		(*c++) = decoder_read_byte(this, &err)
			or_go(clean_up);
	}
	return string;

clean_up:
	free(string);
	fail(NULL, err);
}

Decoder *decoder_create(DecoderSource *source) {
	Decoder *decoder = malloc(sizeof(Decoder));
	decoder->source = source;
	return decoder;
}

void decoder_verify_header(Decoder *this, SepError *out_err) {
	static char EXPECTED_HEADER[] = "SEPT";

	SepError err = NO_ERROR;

	// read enough bytes to verify the header and compare
	// with expected - any mismatch ends up with
	// ENotSeptemberFile being raised
	char *c = EXPECTED_HEADER;
	while(*c) {
		uint8_t byte = decoder_read_byte(this, &err);
			or_quit();
		if (byte != *c)
			fail(e_not_september_file());

		c++;
	}
}

ConstantPool *decoder_read_cpool(Decoder *this, SepError *out_err) {
	SepError err = NO_ERROR;

	// read the number of constants and create a pool
	int32_t num_constants = decoder_read_int(this, &err);
		or_quit_with(NULL);
	log("decoder", "Reading a pool of %d constants.", num_constants);
	ConstantPool * pool = cpool_create(num_constants);

	// fill the pool
	int index;
	for (index = 0; index < num_constants; index++) {
		uint8_t constant_type = decoder_read_byte(this, &err);
			or_quit_with(pool);
		switch(constant_type) {
			case CT_INT:
			{
				SepInt integer = decoder_read_int(this, &err);
					or_quit_with(pool);
				cpool_add_int(pool, integer);
				break;
			}

			case CT_STRING:
			{
				char *string = decoder_read_string(this, &err);
					or_quit_with(pool);
				cpool_add_string(pool, string);
				log("decoder", "constant %d: %s", index+1, string);
				break;
			}

			default:
				fail(pool, e_malformed_module_file("Unrecognized constant type tag"));
		}
	}

	// return the filled pool
	return pool;
}

void decoder_read_block_args(Decoder *this, BlockPool *pool, int arg_count, SepError *out_err) {
	//SepError err = NO_ERROR;
	if (arg_count != 0)
		fail(e_not_implemented_yet("Block arguments not supported yet."));

	bpool_start_block(pool, arg_count);
}

void decoder_read_block_code(Decoder *this, BlockPool *pool, SepError *out_err) {
	SepError err = NO_ERROR;
	
	uint8_t opcode = decoder_read_byte(this, &err);
		or_quit();
	
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
					or_quit();
				break;
			case OP_LAZY_CALL:
				// count arguments
				op_arg_count = decoder_read_byte(this, &err);
					or_quit();
				bpool_write_code(pool, op_arg_count);
				// read them all
				for (arg_index = 0; arg_index < op_arg_count; arg_index++) {
					bpool_write_code(pool, (int16_t)decoder_read_int(this, &err));
						or_quit();
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
			or_quit();
	}
	
	// close the code block
	bpool_end_block(pool);
}

BlockPool *decoder_read_bpool(Decoder *this, SepModule *module, SepError *out_err) {
	SepError err = NO_ERROR;
	
	BlockPool *blocks = bpool_create(module, 2048);
	
	// read functions until we run out 
	uint8_t function_header_byte = decoder_read_byte(this, &err);
		or_quit_with(blocks);
		
	while (function_header_byte != 0xFF) {
		// if not 0xFF, it's the argument count
		uint8_t argument_count = function_header_byte;
		// read the function
		decoder_read_block_args(this, blocks, argument_count, &err);
		decoder_read_block_code(this, blocks, &err);
		
		// next!
		function_header_byte = decoder_read_byte(this, &err);
			or_quit_with(blocks);
	}
	
	// seal the block to make it usable
	bpool_seal(blocks);
	
	return blocks;
}

void decoder_read_module(Decoder *this, SepModule *module, SepError *out_err) {
	SepError err = NO_ERROR;

	decoder_verify_header(this, &err);
		or_quit();
		
	module->constants = decoder_read_cpool(this, &err);
		or_quit();
	module->blocks = decoder_read_bpool(this, module, &err);
		or_quit();
}

void decoder_free(Decoder *this) {
	if (!this) return;

	if (this->source)
		this->source->free(this->source);
	free(this);
}
