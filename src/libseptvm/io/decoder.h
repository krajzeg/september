#ifndef _SEP_DECODER_H
#define _SEP_DECODER_H

/*****************************************************************
 **
 ** io/decoder.h
 **
 ** Code responsible from reading the binary September module files
 ** and decoding them into SepModules.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================


#include "../vm/module.h"

// ===============================================================
//  Decoder object
// ===============================================================

struct ByteSource;
typedef struct BytecodeDecoder {
	// the source from which we are loading the module
	struct ByteSource *source;
} BytecodeDecoder;

// Creates a new decoder pulling bytecode from a provided source.
BytecodeDecoder *decoder_create(struct ByteSource *source);
// Reads the constant pool and codeblock pool from the bytecode.
void decoder_read_pools(BytecodeDecoder *this, SepModule *module, SepV *error);
// Destroys the decoder instance.
void decoder_free(BytecodeDecoder *this);

/*****************************************************************/

#endif
