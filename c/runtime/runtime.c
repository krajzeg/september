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

#include "objectp.h"
#include "stringp.h"

// ===============================================================
//  Prototype objects
// ===============================================================

SepObj *proto_Object;
SepObj *proto_String;

// ===============================================================
//  Built-in functions
// ===============================================================

SepItem print_impl(SepObj *parameters, ExecutionFrame *frame) {

	SepV object_to_print = props_get_prop(parameters, sepstr_create("what"));
	if (!sepv_is_str(object_to_print)) {
		return si_exception(NULL, sepstr_create("print() expects a string for now."));
	}
	const char *string_to_print = sepstr_to_cstr(sepv_to_str(object_to_print));

	printf("%s\n", string_to_print);

	return item_rvalue(SEPV_NOTHING);
}

void introduce_builtins(SepObj *scope) {
	// === various prototypes
	proto_Object = create_object_prototype();
	proto_String = create_string_prototype();

	// === built in variables
	props_add_field(scope, "version", sepv_string("0.1-adder"));

	// === built in functions
	builtin_add(scope, "print", &print_impl, 1, "what");
}
