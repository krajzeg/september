#ifndef _SEP_DEBUGGING_H
#define _SEP_DEBUGGING_H

/***************************************************************/

#ifdef SEP_LOGGING_ENABLED
#define log(module, format, ...) debug_log("[" module "]", format, __VA_ARGS__)
#define log0(module, msg) debug_log("[" module "]", msg)
#else 
#define log(module, format, ...)
#define log0(module, msg)
#endif 

/***************************************************************/

extern char sep_debugged_modules[4096];

void debug_module(const char *module);
void debug_log(const char *module, const char *format, ...);

/***************************************************************/

#endif
