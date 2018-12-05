/**
 * Wrapper of Duktape JS, used as bridge for other language.
 * Rosbit Xu <me@rosbit.cn>
 * Oct. 18, 2018
 */
#ifndef _DUK_BRIDGE_H_
#define _DUK_BRIDGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/** format value for describle args/argv */
typedef enum {
	af_none    = 'n',
	af_bool    = 'b',
	af_int     = 'i',
	af_double  = 'd',
	af_zstring = 's',
	af_lstring = 'S',
	af_buffer  = 'B',
	af_jarray  = 'a',
	af_jobject = 'o',
	af_ecmafunc= 'F'
} arg_format_t;

/** type value for describe fn_native_func() argument `res` */
typedef enum {
	rt_none,   // null or undefined
	rt_bool,   // true/false
	rt_int,    // integer
	rt_double, // double
	rt_string, // const char*, and res_len set to the length of returned value at the same time.
	rt_object, // const char* pointer to a JSON string, and res_len set at the same time.
	rt_buffer, // binary data, same as rt_string, but binary data can contain any character.
	rt_array,  // same as rt_object, but the JSON string is converted from array.
	rt_func,   // ecmascript function object.
	total_rt
} res_type_t;

/**
 * prototype of function to receive the result when calling a JS function.
 * it is the argument `call_func_res` of functions js_call_registered_func()/js_eval()/js_eval_file().
 * @param udd      the `udd` argument when calling js_call_registered_func()/js_eval()/js_eval_file()
 * @param res_type type value to describe `res`, any value must be casted to void*
 *                   rt_none: value in res can be ignored
 *                   rt_bool: value in res is boolean 1/0
 *                   rt_double: value in res is of C double
 *                   rt_string: value in res is the address of a string, and value in res_len is the bytes count
 *                   rt_object: values in res and res_len are pointer and length of a string in JSON format.
 *                   rt_func: value in res is an ecmascript function,
 *                            which must be released by calling js_destroy_ecmascript_func()
 * @param res      the pointer to the result. the result is in JSON format
 * @param res_len  count of bytes in res.
 */
typedef void (*fn_call_func_res)(void *udd, res_type_t res_type, void *res, size_t res_len);

/**
 * to create a environment of JS. the returned result will be used as an argument of the other functions.
 * @param mod_path   the path with a subdir of `modules` (`mod_path`/modules) to store the js modules. 
 *                   If NULL is given, the dir path of the executive will be used.
 * @return  the JS environment.
 */
void* js_create_env(const char *mod_path);

/**
 * to destroy the JS environment.
 * @param env   the result when calling js_create_env()
 */
void js_destroy_env(void *env);

/**
 * destroy ecmascript object. it is used as a base of some macros
 * @param env       the result when calling js_create_env()
 * @param ecma_obj  the ecmascript function or module
 */
void js_destroy_ecmascript_obj(void *env, void *ecma_obj);

/**
 * prototype of a function to read a file content.
 * @param file_name   the name of file to be read
 * @param content     the pointer to the momery allocated by this function, and will be freed by the calling function.
 * @param len         the pointer to the bytes length in `content`
 * @return 0 if succssful, otherwise other value.
 */
typedef int (*fn_read_file)(const char *file_name, char **content, size_t *len);

/**
 * set the function to read a file. if this function is not called, a default implementation to read file from
 * a file system will be used. this function can be called at any time.
 */
void js_set_readfile(fn_read_file read_file);

/**
 * register a funtion in JS file as a global function, which could be call by the func_name.
 * @param env          the result when calling js_create_env()
 * @param script_file  the JS file name
 * @param func_name    the function name to be refered later
 * @return 0 if successfuly, otherwise <0
 */
int js_register_file_func(void *env, const char *script_file, const char *func_name);

/**
 * register a funtion in JS code as a global function, which could be call by the func_name.
 * @param env          the result when calling js_create_env()
 * @param js_code      the string pointer to JS code.
 * @param len          the length of bytes in js_code
 * @param func_name    the function name to be refered later
 * @return 0 if successfuly, otherwise <0
 */
int js_register_code_func(void *env, const char *js_code, size_t len, const char *func_name);

/**
 * unregister the function registered by calling js_register_file_func()/js_register_code_func() before
 * @param env          the result when calling js_create_env()
 * @param func_name    the function name registered.
 * @return 0 if successfuly, otherwise <0
 */
int js_unregister_func(void *env, const char *func_name);

/**
 * call a registered function with arguments
 * @param env           the result when calling js_create_env()
 * @param func_name     the function name resgistered when calling js_register_file_func()/js_register_code_func()
 * @param call_func_res the function to receive the result
 * @param udd           the UDD which will be sent to call_func_res()
 * @param fmt           the format of arguments in argv
 *                        'n' -> None, the corresponding value in argv could be ignored
 *                        'b' -> boolean, the corresponding value in argv could be 1 or 0
 *                        'i' -> integer, the corresponding value in argv is of C int
 *                        'd' -> double, the corresponding value in argv is of C double
 *                        's' -> string, the corresponding value in argv is the address of C z-string
 *                        'S' -> bytes buffer, the next 2 values in argv are length and address of buffer
 *                        'a' -> JS array, the next 2 values in argv are length and address of a string encoded in JSON
 *                        'o' -> JS object, the next 2 values in argv are length and address of a string encoded in JSON
 *                        'F' -> ecmascript function, the corresponding value in argv is an ecmascript function object
 * @param argv          arguments describe by fmt.
 * @return 0 if successfuly, otherwise <0
 */
int js_call_registered_func(void *env, const char *func_name, fn_call_func_res call_func_res, void *udd, char *fmt, void *argv[]);

/**
 * load a JS script file containing only one function and run it with arguments, get the result.
 * @param env           the result when calling js_create_env()
 * @param script_file   the name of the script file which containing only one function, any function name is ok.
 * @param call_func_res the function to receive the result
 * @param udd           the UDD which will be sent to call_func_res()
 * @param fmt           the format of arguments in argv
 *                        'n' -> None, the corresponding value in argv could be ignored
 *                        'b' -> boolean, the corresponding value in argv could be 1 or 0
 *                        'i' -> integer, the corresponding value in argv is of C int
 *                        'd' -> double, the corresponding value in argv is of C double
 *                        's' -> string, the corresponding value in argv is the address of C z-string
 *                        'S' -> bytes buffer, the next 2 values in argv are length and address of buffer
 *                        'a' -> JS array, the next 2 values in argv are length and address of a string encoded in JSON
 *                        'o' -> JS object, the next 2 values in argv are length and address of a string encoded in JSON
 *                        'F' -> ecmascript function, the corresponding value in argv is an ecmascript function object
 * @param argv          arguments describe by fmt.
 * @return 0 if successfuly, otherwise <0
 */
int js_call_file_func(void *env, const char *script_file, fn_call_func_res call_func_res, void *udd, char *fmt, void *argv[]);

/**
 * to evaluate(run) JS code
 * @param env       the result when calling js_create_env()
 * @param js_code   the JS code to run
 * @param len       the bytes length of js_code
 * @param call_func_res the function to receive the result
 * @param udd           the UDD which will be sent to call_func_res()
 * @return 0 if successfuly, otherwise <0
 */
int js_eval(void *env, const char *js_code, size_t len, fn_call_func_res call_func_res, void *udd);

/**
 * to evaluate(run) JS code in a file
 * @param env           the result when calling js_create_env()
 * @param script_file   the JS file in which the codes wil be run
 * @param call_func_res the function to receive the result
 * @param udd           the UDD which will be sent to call_func_res()
 * @return 0 if successfuly, otherwise <0
 */
int js_eval_file(void *env, const char *script_file, fn_call_func_res call_func_res, void *udd);

/* ====================  registering global function related =================== */
/**
 * the finalizer for the returned value of fn_native_func.
 * @param res   the returned value of fn_native_func()
 */
typedef void (*fn_free_res)(void*);

/**
 * the prototype of callback function when calling js_register_native_func().
 * @param fmt               the format of argument args:
 *                            'n' -> None, the corresponding value in args could be ignored
 *                            'b' -> boolean, the corresponding value in args could be 1 or 0
 *                            'd' -> double, the corresponding value in args is C double
 *                            'S' -> string, the next 2 values in args are length and address of string
 *                            'B' -> bytes buffer, the next 2 values in args are length and address of buffer
 *                            'a' -> JS array, the next 2 values in args are length and address of a string encoded in JSON
 *                            'o' -> JS object, the next 2 values in args are length and address of a string encoded in JSON
 *                            'F' -> Ecmascript function object, the correspoinding value in args is void*,
 *                                   which will be called by using js_call_ecmascript_func()
 *                                   and released by calling js_destroy_ecmascript_func()
 * @param args              the args described by fmt
 * @param [OUT]res          the address to store result, data in any type must be casted to void*
 * @param [OUT]res_type     the type of returned value
 * @param [OUT]res_len      the length of returned value if the res_type is rt_string/rt_object
 * @param [OUT]free_res  the finalizer of returned value. NULL if no finalizer.
 */
typedef void (*fn_native_func)(void *udd, const char* fmt, void *args[], void** res, res_type_t *res_type, size_t *res_len, fn_free_res *free_res);

/**
 * to register a function which could be used as a JS global function.
 * @param env           the result when calling js_create_env()
 * @param func_name     the name of function used in JS code.
 * @param native_func   the function pointer really called.
 * @param param_num     the number of arguments for cb. -1 if cb supports variadic arguments.
 * @return 0 if successful, <0 otherwise.
 */
int js_register_native_func(void *env, const char *func_name, fn_native_func native_func, int param_num, void *udd);

/**
 * to unregister a global function registered by calling js_register_native_func()
 * @param env           the result when calling js_create_env()
 * @param func_name     the name of function to be unregistered
 * @return 0 if successful, <0 otherwise.
 */
int js_unregister_native_func(void *env, const char *func_name);

/**
 * call a ecmascript function with arguments.
 * @param env           the result when calling js_create_env()
 * @param ecma_func     the ecmascript function object which was transfered by calling fn_native_func() in js.
 * @param call_func_res the function to receive the result
 * @param udd           the UDD which will be sent to call_func_res()
 * @param fmt           the format of arguments in argv
 *                        'n' -> None, the corresponding value in argv could be ignored
 *                        'b' -> boolean, the corresponding value in argv could be 1 or 0
 *                        'i' -> integer, the corresponding value in argv is of C int
 *                        'd' -> double, the corresponding value in argv is of C double
 *                        's' -> string, the corresponding value in argv is the address of C z-string
 *                        'S' -> bytes buffer, the next 2 values in argv are length and address of buffer
 *                        'a' -> JS array, the next 2 values in argv are length and address of a string encoded in JSON
 *                        'o' -> JS object, the next 2 values in argv are length and address of a string encoded in JSON
 *                        'F' -> ecmascript function, the corresponding value in argv is an ecmascript function object
 * @param argv          arguments describe by fmt.
 * @return 0 if successfuly, otherwise <0
 */
int js_call_ecmascript_func(void *env, void *ecma_func, fn_call_func_res call_func_res, void *udd, char *fmt, void *argv[]);

/**
 * release the saved ecmascript function.
 * @param ecma_func   the ecmascript function object which was transfered by calling fn_native_func() in js.
 */
#define js_destroy_ecmascript_func(env, ecma_func) js_destroy_ecmascript_obj(env, ecma_func)

/* =================== utils to convert between double and void* ================== */
void* double2voidp(double);
double voidp2double(void*);

/* ================== module loader =================*/
/** module method definition */
typedef struct {
	const char *name;       // method name to call this method in the format `mod_name`.`name`()
	fn_native_func method;  // the native method 
	int nargs;              // the number of arguments of the method, <0 if the method supports variadic arguments
	void *udd;              // the udd transfered to method arguments
} module_method_t;

/** module attribute definition */
typedef struct {
	const char *name; // attribute name to refered in the format `mod_name`.`name`
	arg_format_t fmt; // the format of type of the attribute value. please refer to js_call_registered_func()
	void *val;        // the value of attribute.
	size_t val_len;   // the length of bytes in val
} module_attr_t;

/**
 * prototype of loading module. this function is called by a module loader.
 * @param udd        argument when calling js_add_module_loader()
 * @param mod_home   the home path to load a native module
 * @param mod_name   the name of module to be loaded
 * @return  the module handle if successful, otherwise zero. value must be casted to void*
 */
typedef void* (*fn_load_module)(void *udd, const char *mod_home, const char *mod_name);
/**
 * prototype of get method list of the module. methods will be attached to the given module.
 * If memory controlling restriction, this function can call js_add_module_method() directly and return NULL.
 * @param udd         argument when calling js_add_module_loader()
 * @param mod_name    the name of module to be loaded
 * @param mod_handle  the result of fn_load_module(), or argument of js_create_ecmascript_module()
 * @return  a pointer to a module_method_t array with the last item set to zero.
 */
typedef module_method_t* (*fn_get_methods_list)(void *udd, const char *mod_name, void* mod_handle);
/**
 * prototype of get attribute list of the module. attributes will be attached to the given module.
 * @param udd         argument when calling js_add_module_loader()
 * @param mod_name    the name of module to be loaded
 * @param mod_handle  the result of fn_load_module(), or argument of js_create_ecmascript_module()
 * @return  a pointer to a module_attr_t array with the last item set to zero.
 */
typedef module_attr_t* (*fn_get_attrs_list)(void *udd, const char *mod_name, void* mod_handle);
/**
 * finalizer of a module when it reaches the end of its block scope.
 * @param udd         argument when calling js_add_module_loader()
 * @param mod_name    the name of module to be loaded
 * @param mod_handle  the result of fn_load_module(), or argument of js_create_ecmascript_module()
 */
typedef void (*fn_module_finalizer)(void *udd, const char *mod_name, void *mod_handle);

/**
 * to register a module loader. module loaders will be called when calling `require(mod_name)` in JS.
 * @param env              the result when calling js_create_env()
 * @param udd              argument which will be transfered to load_moudle()/get_methods_list()/get_attrs_list()/finalizer()
 * @param mod_ext          the module file extension for this module loader
 * @param load_module      the init function to init a module instance
 * @param get_methods_list the function to return the methods list
 * @param get_attrs_list   the function to return the attributes list
 * @param finalizer        the function when the module instance reaches the end of its scope.
 */
void js_add_module_loader(void *env, void *udd, const char *mod_ext, fn_load_module load_module, fn_get_methods_list get_methods_list, fn_get_attrs_list get_attrs_list, fn_module_finalizer finalizer);

/**
 * a helper function which will be used to attach ONE method in a list returned by calling get_methods_list().
 * @param env        the result when calling js_create_env()
 * @param method     the method description
 */
void js_add_module_method(void *env, module_method_t *method);

/**
 * a helper function which will be used to attach ONE attribute in a list returned by calling get_attrs_list().
 * @param env      the result when calling js_create_env()
 * @param attr     the attribute description
 */
void js_add_module_attr(void *env, module_attr_t *attr);

/**
 * create a ecmascript module which can be used as a ecmascript function arguement.
 * @param env              the result when calling js_create_env()
 * @param udd              argument which will be transfered to get_methods_list()/get_attrs_list()/finalizer()
 * @param mod_handle       as an argument of get_methods_list()/get_attr_list()/finalizer()
 * @param get_methods_list the function to return the methods list
 * @param get_attrs_list   the function to return the attributes list
 * @param finalizer        the function when the module instance reaches the end of its scope.
 * @return  non-NULL if successful, otherwise NULL.
 *          The non-NULL result can be used as a ecmascript function.
 *          Release it by calling js_destroy_ecmascript_module() when it is not used any more.
 */
void* js_create_ecmascript_module(void *env, void *udd, void *mod_handle, fn_get_methods_list get_methods_list, fn_get_attrs_list get_attrs_list, fn_module_finalizer finalizer);

/**
 * destroy module created by calling js_create_ecmascript_module()
 * @param env     the result when calling js_create_env()
 * @param module  the result of js_create_ecmascript_module()
 */
#define js_destroy_ecmascript_module(env, module) js_destroy_ecmascript_obj(env, module)

#ifdef __cplusplus
}
#endif

#endif
