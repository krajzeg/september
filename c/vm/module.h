#ifndef _SEP_RUNTIME_MODULE_H
#define _SEP_RUNTIME_MODULE_H

/*****************************************************************
 **
 ** runtime/module.h
 **
 ** Contains everything that is needed to represent a September
 ** module in memory. This includes the types representing the
 ** module itself and the relevant memory pools for constants
 ** and functions.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
// Includes
// ===============================================================

#include "types.h"
#include "code.h"

// ===============================================================
// Pre-declaring structs
// ===============================================================

struct ConstantPool;
struct BlockPool;
struct SepModule;
struct SepObj;

// ===============================================================
// Module type
// ===============================================================

/**
 Represents a fully loaded module. Each module is named, and consists
 of two pools - the constant pool (which houses all non-function
 constants used in the code) and the function pool (which houses all
 code comprising the module). Both pools are indexed, and constants
 and functions are accessed by their index.

 Upon loading, every module starts execution from function #0, which
 is always parameterless. A new SepObj is created to house the module,
 and is the scope in which this function will execute.
 **/
typedef struct SepModule {
    // the name of this module, dynamically allocated
    char *name;
    // the constant pool used by the code
    struct ConstantPool *constants;
    // the block pool for the code in this module
    struct BlockPool *blocks;
    // the root object of this module
    struct SepObj *root;
} SepModule;

// Creates a new empty module.
SepModule *module_create(struct SepObj *globals, struct SepObj *syntax);
// Deinitializes and frees the memory taken by this module.
void module_free(SepModule *this);

// ===============================================================
// Constant pools
// ===============================================================

/**
 Represents a constant pool. Has to be initialized with the number
 of constants it will be housing. Allocates a single block of
 memory with 8-bytes for each constant (since each constant is
 represented by an 8-byte SepV, see types.h). This block will grow
 dynamically to accomodate constants that need additional storage,
 like strings.
 **/
typedef struct ConstantPool {
    // the number of constants already added to the pool
	uint32_t constant_count;
	// the number of constants the pool will eventually have
	uint32_t max_constants;
	// pointer to the memory used to store the constants and the
	// end of this memory
	uint8_t *data, *data_end_ptr;
	// always points to the place where additional storage for
	// constants will be next allocated
	uint8_t *data_alloc_ptr;
} ConstantPool;

// Creates a new constant pool capable of housing 'num_constants'
// values.
ConstantPool *cpool_create(uint32_t num_constants);

// Adds a new string constant at the next index.
SepV cpool_add_string(ConstantPool *this, const char *c_string);
// Adds a new integer constant at the next index.
SepV cpool_add_int(ConstantPool *this, SepInt index);
// Fetches a constant under a given 'index' from the pool.
SepV cpool_constant(ConstantPool *this, uint32_t index);

// Frees the constant pool and all its contents.
void cpool_free(ConstantPool *this);

// ===============================================================
//  Block pools
// ===============================================================

/**
 * Block pools store blocks of September bytecode representing all
 * the code in a module. Each block is identified by an index.
 * During execution, those blocks are used to instantiate
 * InterpretedFunc objects, which marry the code itself with
 * additional context (like a 'this' scope) to make it
 * executable.
 */
typedef struct BlockPool {
	// the module it belongs to
	SepModule *module;
	// the block of memory where blocks are stored
	uint8_t *memory, *memory_end;
	// total number of blocks in this pool
	uint16_t total_blocks;
	// an index created when the pool is sealed, allowing
	// for quick access to the blocks
	CodeBlock **block_index;
	// the block currently under construction
	CodeBlock *current_block;
	// write pointer - points at the position the next code unit
	// will be placed
	uint8_t *position;
} BlockPool;

// Creates a new, empty block pool.
BlockPool *bpool_create(SepModule *module, uint32_t initial_memory_size);
// Starts a new code block.
void bpool_start_block(BlockPool *this, int parameter_count);
// Writes a single operation or its argument to the current block's
// instruction stream.
void bpool_write_code(BlockPool *this, CodeUnit code);
// Terminates the current block and returns its index.
uint32_t bpool_end_block(BlockPool *this);
// Seals the pool - no more blocks can be added after this point.
// The block pool is not usable before it's sealed.
void bpool_seal(BlockPool *this);
// Retrieves a CodeBlock from the pool.
CodeBlock *bpool_block(BlockPool *this, uint32_t index);
// Frees a block pool.
void bpool_free(BlockPool *this);

/******************************************************************/

#endif
