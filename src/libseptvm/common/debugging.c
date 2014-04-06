/***************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "debugging.h"
#include "../vm/vm.h"

/**************************************************/

void debug_module(const char *module) {
	strcat(lsvm_globals.debugged_module_names, "[");
	strcat(lsvm_globals.debugged_module_names, module);
	strcat(lsvm_globals.debugged_module_names, "]");
}

void debug_log(const char *module, const char *format, ...) {
	if (!strstr(lsvm_globals.debugged_module_names, module))
		return;

	printf("%12s ", module);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    puts("");
}
