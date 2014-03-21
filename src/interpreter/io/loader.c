/*****************************************************************
 **
 ** loader.c
 **
 ** Implements the module loader API from loader.h
 **
 ***************
 ** September **
 *****************************************************************/


// ===============================================================
//  Includes
// ===============================================================

#include "loader.h"

// ===============================================================
//  File sources
// ===============================================================

typedef struct FileSource {
	ByteSourceVT *vt;
	FILE *file;
} FileSource;

// Gets the next byte from the file connected with the file source.
uint8_t filesource_get_byte(ByteSource *_this, SepError *out_err) {
	FileSource *this = (FileSource*)_this;
	int byte = fgetc(this->file);
	if (byte == EOF)
		fail(0, e_unexpected_eof());
	return byte;
}

// Closes the file source along with its file.
void filesource_close_and_free(ByteSource *_this) {
	FileSource *this = (FileSource*)_this;

	// close and free everything safely
	if (!this) return;
	if (this->file)
		fclose(this->file);
	free(this);
}

ByteSourceVT filesource_vt = {
	&filesource_get_byte, &filesource_close_and_free
};

// Creates a new file source pulling from a given file.
ByteSource *file_bytesource_create(const char *filename, SepError *out_err) {
	// try opening the file
	FILE *file = fopen(filename, "rb");
	if (file == NULL)
		fail(NULL, e_file_not_found(filename));

	// allocate and set up the data structure
	FileSource *source = malloc(sizeof(FileSource));
	source->vt = &filesource_vt;
	source->file = file;

	return source;
}
