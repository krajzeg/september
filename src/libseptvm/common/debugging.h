#ifndef _SEP_DEBUGGING_H
#define _SEP_DEBUGGING_H

/***************************************************************/

// A set of macros that have an effect only if debugging is turned on.

#ifdef SEP_DEBUG
#define log(module, format, ...) debug_log("[" module "]", format, __VA_ARGS__)
#define log0(module, msg) debug_log("[" module "]", msg)
#define debug_only(code) do { code } while(0);
#else 
#define log(module, format, ...)
#define log0(module, msg)
#define debug_only(code)
#endif 

/***************************************************************/

void debug_module(const char *module);
void debug_log(const char *module, const char *format, ...);

/***************************************************************/

#endif
