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
#include "support.h"

// ===============================================================
//  Prototype objects
// ===============================================================

SepObj *proto_Object;
SepObj *proto_String;

// ===============================================================
//  Built-in functions
// ===============================================================

SepItem print_impl(SepObj *scope, ExecutionFrame *frame) {
	SepError err = NO_ERROR;
	SepString *to_print = param_as_str(scope, "what", &err);
		or_raise_with_msg(NULL, "print() only handles strings for now.");

	printf("%s\n", sepstr_to_cstr(to_print));
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
