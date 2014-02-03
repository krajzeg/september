/***************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "debugging.h"

/**************************************************/

char sep_debugged_modules[4096] = "";

/**************************************************/

void debug_module(const char *module) {
	strcat(sep_debugged_modules, "[");
	strcat(sep_debugged_modules, module);
	strcat(sep_debugged_modules, "]");	
}

void debug_log(const char *module, const char *format, ...) {
	if (!strstr(sep_debugged_modules, module))
		return;

	printf("%12s ", module);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    puts("");
}
