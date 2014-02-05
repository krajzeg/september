// ===============================================================
//  Includes
// ===============================================================

#include <stdio.h>
#include <stdlib.h>
#include "../vm/functions.h"
#include "../vm/types.h"
#include "../vm/objects.h"
#include "../vm/exceptions.h"
#include "../vm/vm.h"

#include "support.h"

// ===============================================================
//  Prototype objects
// ===============================================================

SepObj *proto_Object;
SepObj *proto_String;
SepObj *proto_Integer;

// ===============================================================
//  Prototype creation methods
// ===============================================================

SepObj *create_object_prototype();
SepObj *create_integer_prototype();
SepObj *create_string_prototype();

// ===============================================================
//  Built-in functions
// ===============================================================

// hack for MinGW's strange printf situation
#ifdef __MINGW32__
#define printf __mingw_printf
#endif

SepItem print_impl(SepObj *scope, ExecutionFrame *frame) {
	SepV to_print = param(scope, "what");

	if (sepv_is_str(to_print)) {
		printf("%s\n", sepstr_to_cstr(sepv_to_str(to_print)));
	} else if (sepv_is_int(to_print)) {
		printf("%lld\n", sepv_to_int(to_print));
	} else {
		raise(NULL, "print() supports strings and ints for now.");
	}
	return item_rvalue(SEPV_NOTHING);
}

void initialize_runtime(SepObj *scope) {
	// "Object" has to be initialized first, as its the prototype to all other prototypes
	proto_Object = create_object_prototype();

	// primitive types' prototypes are initialized here
	proto_Integer = create_integer_prototype();
	proto_String = create_string_prototype();

	// built-in variables are initialized
	obj_add_field(scope, "version", sepv_string("0.1-aeon"));

	// built-in functions are initialized
	obj_add_builtin_func(scope, "print", &print_impl, 1, "what");
}
