#ifndef _SEP_RUNTIME_SUPPORT_H_
#define _SEP_RUNTIME_SUPPORT_H_

/*****************************************************************
 **
 ** runtime/support.h
 **
 ** Various functions and macros helpful when writing September
 ** built-in methods and prototypes.
 **
 ***************
 ** September **
 *****************************************************************/

// ===============================================================
//  Includes
// ===============================================================

#include "../vm/types.h"
#include "../vm/exceptions.h"
#include "../vm/vm.h"

// ===============================================================
//  Error handling primitives
// ===============================================================

#define raise(exc_type, ...) return si_exception(exc_type, sepstr_sprintf(__VA_ARGS__));

#define or_raise(exc_type) if (err.type && (!err.handled)) { return si_exception(exc_type, sepstr_create(err.message)); }
#define or_raise_with_msg(exc_type, ...) if (err.type && (!err.handled)) { return si_exception(exc_type, sepstr_sprintf(__VA_ARGS__)); }

#define or_propagate(subcall_result) if (sepv_is_exception(subcall_result)) { return item_rvalue(subcall_result); }

// ===============================================================
//  Accessing this and parameters
// ===============================================================

#define target(scope) param(scope, "this")
#define target_as_str(scope, err_ptr) _target_as(scope, str, err_ptr)
#define target_as_obj(scope, err_ptr) _target_as(scope, obj, err_ptr)
#define target_as_func(scope, err_ptr) _target_as(scope, func, err_ptr)
#define target_as_int(scope, err_ptr) _target_as(scope, int, err_ptr)
#define _target_as(scope, desired_type, err_ptr) cast_as_named_##desired_type("Target object", target(scope), err_ptr)

#define param(scope, param_name) (props_get_prop(scope, sepstr_create(param_name)))
#define param_as_str(scope, param_name, err_ptr) _param_as(scope, param_name, str, err_ptr)
#define param_as_obj(scope, param_name, err_ptr) _param_as(scope, param_name, obj, err_ptr)
#define param_as_func(scope, param_name, err_ptr) _param_as(scope, param_name, func, err_ptr)
#define param_as_int(scope, param_name, err_ptr) _param_as(scope, param_name, int, err_ptr)

#define _param_as(scope, param_name, desired_type, err_ptr) cast_as_named_##desired_type("Parameter '" param_name "'", param(scope, param_name), err_ptr)

// ===============================================================
//  Casting
// ===============================================================

SepString *cast_as_str(SepV value, SepError *out_err);
SepObj *cast_as_obj(SepV value, SepError *out_err);
SepFunc *cast_as_func(SepV value, SepError *out_err);
SepInt cast_as_int(SepV value, SepError *out_err);

SepString *cast_as_named_str(char *name, SepV value, SepError *out_err);
SepObj *cast_as_named_obj(char *name, SepV value, SepError *out_err);
SepFunc *cast_as_named_func(char *name, SepV value, SepError *out_err);
SepInt cast_as_named_int(char *name, SepV value, SepError *out_err);

// ===============================================================
//  Classes
// ===============================================================

// Creates a new class object with the given name and parent class.
SepObj *make_class(char *name, SepObj *parent);

// ===============================================================
//  Exception access
// ===============================================================

// Returns a built-in exception type.
SepObj *builtin_exception(char *name);

// ===============================================================
//  Retrieving properties quickly
// ===============================================================

// Retrieves a property from a SepV using proper lookup.
// Function simplifying property retrieval with constant name,
// used instead of sepv_get when you don't have a SepStr.
SepV property(SepV host, char *name);
// Retrieves a property and casts it to a SepObj*.
#define prop_as_obj(host, name, err) (cast_as_named_obj("Property '" name "'", property(host, name), err))
// Retrieves a property and casts it to a SepFunc*.
#define prop_as_func(host, name, err) (cast_as_named_func("Property '" name "'", property(host, name), err))
// Retrieves a property and casts it to an integer.
#define prop_as_int(host, name, err) (cast_as_named_int("Property '" name "'", property(host, name), err))
// Retrieves a property and casts it to a string.
#define prop_as_str(host, name, err) (cast_as_named_str("Property '" name "'", property(host, name), err))

// ===============================================================
//  Adding properties
// ===============================================================

// Adds a new field to a given object.
void obj_add_field(SepObj *obj, char *name, SepV contents);
// Adds a new built-in method to a given object.
void obj_add_builtin_method(SepObj *obj, char *name, BuiltInImplFunc impl, uint8_t param_count, ...);
// Adds a new built-in free function (as opposed to a method) to a given object.
void obj_add_builtin_func(SepObj *obj, char *name, BuiltInImplFunc impl, uint8_t param_count, ...);
// Adds a new prototype to the object.
void obj_add_prototype(SepObj *obj, SepV prototype);
// Adds a new escape function (like "return", "break", etc.) to a local scope.
void obj_add_escape(SepObj *obj, char *name, ExecutionFrame *return_to_frame, SepV return_value);

// ===============================================================
//  Escape functions
// ===============================================================

// Makes a new escape function. When an escape function is called, it pops
// frames from the VM's execution stack until it reaches the designated
// frame. Once it does, it will exit this frame with the provided return
// value. This is used to implement 'break', 'continue' and 'return'
// "keywords".
BuiltInFunc *make_escape_func(ExecutionFrame *frame, SepV value_returned);
BuiltInFunc *make_return_func(ExecutionFrame *frame);

// ===============================================================
//  Common operations
// ===============================================================

SepString *sepv_debug_string(SepV sepv, SepError *out_err);

/*****************************************************************/

#endif
