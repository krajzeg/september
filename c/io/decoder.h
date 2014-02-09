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

#include "../common/errors.h"
#include "../vm/module.h"

// ===============================================================
//  Source interface
// ===============================================================

/**
 * Decoder sources represent place from which we can load modules.
 * A file is one such source, but we can load from any stream able
 * to give us consecutive bytes.
 */
struct DecoderSource;
typedef uint8_t (*GetNextByteFunc)(struct DecoderSource *, SepError*);
typedef void (*FreeDSFunc)(struct DecoderSource *);
typedef struct DecoderSource {
	// pointer to the correct implementation of get_next_byte
	// for this type of decoder source
	GetNextByteFunc get_next_byte;
	FreeDSFunc free;
} DecoderSource;

// ===============================================================
//  Decoder object
// ===============================================================

typedef struct Decoder {
	// the source from which we are loading the module
	DecoderSource *source;
} Decoder;

Decoder *decoder_create(DecoderSource *source);
void decoder_read_module(Decoder *this, SepModule *module, SepError *out_err);
void decoder_free(Decoder *this);

// ===============================================================
//  File-based decoder source
// ===============================================================

typedef struct FileSource {
	// base contents that all DecoderSources share
	struct DecoderSource base;
	// the file we are reading from
	FILE *file;
} FileSource;

FileSource *filesource_open(const char *filename, SepError *err);
void filesource_close(FileSource *this);

/*****************************************************************/

#endif
