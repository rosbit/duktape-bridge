/**
 * Wrapper of Duktape JS, used as bridge for other language
 * Rosbit Xu <me@rosbit.cn>
 * Oct. 18, 2018
 */
#include "duktape.h"
#include "duk_print_alert.h"
#include "duk_console.h"
#include "duk_bridge.h"
#include "duk_module_duktape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <libgen.h>
#include <dlfcn.h>
#include <sys/time.h>
#ifdef Darwin
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

#define NATIVE_FUNC "_nf_"
#define NATIVE_UDD  "_nu_"

#define NATIVE_MOD  "_nm_"
#define NATIVE_MOD_HANDLE    "_nmh_"
#define NATIVE_MOD_COUNT     "_nmc_"
#define NATIVE_MOD_EXT       "_nme_"
#define NATIVE_MOD_LOADER    "_nml_"
#define NATIVE_MOD_METHODS   "_nmm_"
#define NATIVE_MOD_ATTRS     "_nma_"
#define NATIVE_MOD_FINALIZER "_nmf_"

#define ECMA_OBJ "_eo_"

static char *createHiddenSymbol(const char *type, unsigned long index) {
	char *hiddenSymbol;
	asprintf(&hiddenSymbol, "\xFF%s%lu", type, index); //hidden symbol
	return hiddenSymbol;
}

#define getNativeModNum(index) createHiddenSymbol(NATIVE_MOD, index)
#define getEcmaObjNum(index) createHiddenSymbol(ECMA_OBJ, index)

static void make_func_bridge(duk_context *ctx, const char *func_name, fn_native_func native_func, duk_idx_t nargs, void *udd);

// the default implementation of readFileContent
static int defReadFileContent(const char *f, char **c, size_t *l) {
	struct stat sb;
	if (stat(f, &sb) == -1) {
		// perror(f);
		return -1;
	}
	off_t size = sb.st_size;
	if (size == 0) {
		fprintf(stderr, "%s has no content.\n", f);
		return -2;
	}
	char *src = malloc(size);
	if (src == NULL) {
		fprintf(stderr, "failed to calloc memory of %ld bytes for %s\n", (long)size, f);
		return -3;
	}
	FILE *fp = fopen(f, "rb");
	if (fp == NULL) {
		free(src);
		fprintf(stderr, "failed to open %s for reading\n", f);
		return -4;
	}
	if (fread(src, 1, size, fp) != size) {
		fclose(fp);
		free(src);
		fprintf(stderr, "failed to read %s\n", f);
		return -5;
	}
	fclose(fp);

	*c = src;
	*l = size;
	return 0;
}

static fn_read_file readFileContent = defReadFileContent;

void js_set_readfile(fn_read_file read_file)
{
	if (read_file != NULL) {
		readFileContent = read_file;
	}
}

static duk_ret_t unloadDll(duk_context *ctx) {
	duk_push_current_function(ctx);
	if (duk_get_prop_string(ctx, -1, NATIVE_MOD_HANDLE)) {
		void *hMod = duk_get_pointer(ctx, -1);
		dlclose(hMod);
	}
	duk_pop_2(ctx);
	return 0;
}

static duk_ret_t loadAndInitDll(duk_context *ctx) {
	const char *modPath = duk_get_string(ctx, 0);
	const char *modName = duk_get_string(ctx, 1);

	void *hMod = dlopen(modPath, RTLD_LAZY);
	if (hMod == NULL) {
		duk_push_undefined(ctx);
		return 1;
	}
	char initFunc[40];
	sprintf(initFunc, "init_%s", modName);
	void *initFn = dlsym(hMod, initFunc);
	if (initFn == NULL) {
		dlclose(hMod);
		duk_push_undefined(ctx);
		return 1;
	}

	duk_push_c_function(ctx, (duk_c_function)initFn, 0);
	duk_call(ctx, 0);                                // [ module ]

	duk_push_c_function(ctx, unloadDll, 0);          // [ module, unloadDll ]
	duk_push_pointer(ctx, hMod);                     // [ module, unloadDll, hMod ]
	duk_put_prop_string(ctx, -2, NATIVE_MOD_HANDLE); // [ module, unloadDll ] with unloadDll[NATIVE_MOD_HANDLE] = hMod
	duk_set_finalizer(ctx, -2);                      // [ module ] with unloadDll as finalizer

	// dlclose(hMod);  // this will be called in unloadDll()
	return 1;
}

// unload block scope native module
static duk_ret_t unload_native_obj(duk_context *ctx) {
	duk_push_current_function(ctx);

	duk_get_prop_string(ctx, -1, NATIVE_MOD);
	duk_get_prop_string(ctx, -2, NATIVE_MOD_FINALIZER);
	duk_get_prop_string(ctx, -3, NATIVE_UDD);
	duk_get_prop_string(ctx, -4, NATIVE_MOD_HANDLE);

	const char *mod_name = duk_get_string(ctx, -4);
	fn_module_finalizer finalizer = (fn_module_finalizer)duk_get_pointer(ctx, -3);
	void *udd = duk_get_pointer(ctx, -2);
	void *hMod = duk_get_pointer(ctx, -1);
	duk_pop_n(ctx, 5);

	finalizer(udd, mod_name, hMod);
	return 0;
}

static unsigned long save_top_object(duk_context *ctx) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	unsigned long func_index = (unsigned long)tv.tv_sec * 1000000 + (unsigned long)tv.tv_usec;

	char *funcName = getEcmaObjNum(func_index);
	duk_put_global_string(ctx, funcName);
	free(funcName);
	return func_index;
}

static duk_bool_t load_object(duk_context *ctx, unsigned long func_index) {
	char *funcName = getEcmaObjNum(func_index);
	duk_bool_t r = duk_get_global_string(ctx, funcName);
	free(funcName);
	return r;
}

static void destroy_object(duk_context *ctx, unsigned long func_index) {
	char *funcName = getEcmaObjNum(func_index);
	duk_push_global_object(ctx);
	duk_del_prop_string(ctx, -1, funcName);
	duk_pop(ctx);
	free(funcName);
}

void js_add_module_method(void *env, module_method_t *method)
{
	duk_context *ctx = (duk_context*)env;
	int nargs;
	if (method->nargs < 0) {
		nargs = DUK_VARARGS;
	} else {
		nargs = method->nargs;
	}
	make_func_bridge(ctx, method->name, method->method, nargs, method->udd); // [ ..., obj ] with obj[method->name] = native_func_bridge
}

void js_add_module_attr(void *env, module_attr_t *attr)
{
	// [ ..., obj ]
	duk_context *ctx = (duk_context*)env;
	switch (attr->fmt) {
	case af_none:
		duk_push_null(ctx);
		break;
	case af_bool:
		duk_push_boolean(ctx, (int)(long)attr->val);
		break;
	case af_int:
		duk_push_int(ctx, (int)(long)attr->val);
		break;
	case af_double:
		duk_push_number(ctx, voidp2double(attr->val));
		break;
	case af_zstring:
		duk_push_string(ctx, (char*)attr->val);
		break;
	case af_lstring:
		duk_push_lstring(ctx, (char*)attr->val, attr->val_len);
		break;
	case af_ecmafunc:
		load_object(ctx, (unsigned long)attr->val); // now the top ctx is [ func ]
		break;
	case af_buffer:
	case af_jarray:
	case af_jobject:
	default:
		duk_push_lstring(ctx, (char*)attr->val, attr->val_len);
		duk_json_decode(ctx, -1);
		break;
	}
	duk_put_prop_string(ctx, -2, attr->name); // [ ..., obj ] with obj[attr->name] = attr
}

enum {
	init_obj_ok = 0,
	failed_to_init_obj,
	try_other_init
};

static int init_native_obj(duk_context *ctx, void *udd, const char *mod_home, const char *id, const char *mod_ext, fn_load_module load_module, fn_get_methods_list get_methods_list, fn_get_attrs_list get_attrs_list, fn_module_finalizer finalizer)
{
	void *hMod = NULL;
	if (load_module != NULL) {
		hMod = load_module(udd, mod_home, id);
		if (hMod == NULL) {
			return try_other_init;
		}
	} else {
		hMod = (void*)mod_home;
	}
	duk_push_object(ctx); // a tricky, one can call js_add_module_method() in get_methods_list()

	if (finalizer != NULL) {
		duk_push_c_function(ctx, unload_native_obj, 0);     // [ ..., obj, unload_native_obj ]

		duk_push_string(ctx, id);
		duk_put_prop_string(ctx, -2, NATIVE_MOD);           // [ ..., obj, unload_native_obj ] with unload_native_obj[_nm_] = id 

		duk_push_pointer(ctx, hMod);
		duk_put_prop_string(ctx, -2, NATIVE_MOD_HANDLE);    // [ ..., obj, unload_native_obj ] with unload_native_obj[_nmh_] = hMod

		duk_push_pointer(ctx, udd);
		duk_put_prop_string(ctx, -2, NATIVE_UDD);           // [ ..., obj, unload_native_obj ] with unload_native_obj[_mu_] = udd

		duk_push_pointer(ctx, finalizer);
		duk_put_prop_string(ctx, -2, NATIVE_MOD_FINALIZER); // [ ..., obj, unload_native_obj ] with unload_native_obj[_nmf_] = module_finalizer

		duk_set_finalizer(ctx, -2); // [ ..., obj ] with unload_native_obj as finalizer
	}

	int i;
	if (get_methods_list != NULL) {
		module_method_t *methods = get_methods_list(udd, id, hMod);
		if (methods != NULL) {
			for (i=0; methods[i].name != NULL; i++) {
				js_add_module_method(ctx, methods+i);
			}
		}
	}

	if (get_attrs_list != NULL) {
		module_attr_t *attrs = get_attrs_list(udd, id, hMod);
		if (attrs != NULL) {
			for (i=0; attrs[i].name != NULL; i++) {
				js_add_module_attr(ctx, attrs+i);
			}
		}
	}
	return init_obj_ok;
}

// load module with module loaders
static duk_ret_t loadNativeModule(duk_context *ctx) {
	const char *mod_home = duk_get_string(ctx, 0); 
	const char *id = duk_get_string(ctx, 1);
	int loader_count = 0;

	duk_push_global_object(ctx); // [ ..., global ]
	if (!duk_get_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL(NATIVE_MOD_COUNT))) {
		duk_pop_2(ctx);
		duk_push_undefined(ctx);
		return 1;
	}

	loader_count = duk_get_int(ctx, -1);
	duk_pop_2(ctx);
	// [ ... ]
	int i;
	for (i=0; i<loader_count; i++) {
		char *modNum = getNativeModNum(i);
		duk_bool_t rc = duk_get_global_string(ctx, modNum);
		free(modNum);
		if (!rc) {
			return 1;
		}

		// [ ..., loader ]
		duk_get_prop_string(ctx, -1, NATIVE_MOD_EXT);
		duk_get_prop_string(ctx, -2, NATIVE_MOD_LOADER);
		duk_get_prop_string(ctx, -3, NATIVE_MOD_METHODS);
		duk_get_prop_string(ctx, -4, NATIVE_MOD_ATTRS);
		duk_get_prop_string(ctx, -5, NATIVE_MOD_FINALIZER);
		duk_get_prop_string(ctx, -6, NATIVE_UDD);

		void *udd = duk_get_pointer(ctx, -1);
		fn_module_finalizer finalizer = (fn_module_finalizer)duk_get_pointer(ctx, -2);
		fn_get_attrs_list get_attrs = (fn_get_attrs_list)duk_get_pointer(ctx, -3);
		fn_get_methods_list get_methods = (fn_get_methods_list)duk_get_pointer(ctx, -4);
		fn_load_module load_module = (fn_load_module)duk_get_pointer(ctx, -5);
		const char *ext = duk_get_string(ctx, -6);
		duk_pop_n(ctx, 7);
		// [ ... ]

		if (load_module == NULL || get_methods == NULL) {
			continue;
		}

		int ret = init_native_obj(ctx, udd, mod_home, id, ext, load_module, get_methods, get_attrs, finalizer);
		switch (ret) {
		case init_obj_ok:
			return 1;
		case failed_to_init_obj:
			duk_push_undefined(ctx);
			return 1;
		case try_other_init:
		default:
			continue;
		}
	}

	duk_push_undefined(ctx);
	return 1;
}

static duk_ret_t readFile(duk_context *ctx) {
	const char *modPath = duk_get_string(ctx, 0);

	char *src;
	size_t size;
	int ret = readFileContent(modPath, &src, &size);
	if (ret != 0) {
		duk_push_undefined(ctx);
		return 1;
	}
	duk_push_lstring(ctx, src, size);
	free(src);
	return 1;
}

static void set_global_function(duk_context *ctx, const char *func_name, duk_c_function func, duk_idx_t nargs) {
	duk_push_string(ctx, func_name);       // [ global, func_name ]
	duk_push_c_function(ctx, func, nargs); // [ global, func_name, function]
	duk_put_prop(ctx, -3);                 // [ global ] with global[func_name]=function
}

static void init_global_funcs(duk_context *ctx) {
	duk_push_global_object(ctx);            // [ global ]

	set_global_function(ctx, "readFile", readFile, 1);
	set_global_function(ctx, "loadNativeModule", loadNativeModule, 2);
	set_global_function(ctx, "loadAndInitDll", loadAndInitDll, 2);

	duk_pop(ctx);
}

static void getExePath(char *path, size_t size)
{
#ifdef Darwin
	_NSGetExecutablePath(path, (uint32_t*)&size);
#else
	size = readlink("/proc/self/exe", path, size);
#endif
	path[size] = '\0';
}

// the Duktape.modSearch implementation. there's a '%s' which will be replaced by the value of `mod_home`
static const char *modSearch_impl = "\
Duktape.modSearch = function (id, require, exports, module) {\n\
    /* readFile(): reads a file from disk, and returns a string or undefined.\n\
     * 'id' is in resolved canonical form so it only contains terms and\n\
     * slashes, and no '.' or '..' terms.\n\
     *\n\
     * loadAndInitDll(): load DLL, call its init function, return module\n\
     * init function return value (typically an object containing the\n\
     * module's function/constant bindings).  Return undefined if DLL\n\
     * not found.\n\
     */\n\
    var name;\n\
    var src;\n\
    var mod;\n\
    //print('in modSearch_impl');\n\
	var modHome = '%s/modules/';\n\
	if (id.endsWith('.js')) {\n\
		name = modHome + id;\n\
		src = readFile(name);\n\
		if (typeof src === 'string') {\n\
			return src;\n\
		}\n\
		throw new Error('module not found: ' + id);\n\
	}\n\
	/* ECMAScript check. */\n\
	name = modHome + id + '.js';\n\
	src = readFile(name);\n\
	if (typeof src === 'string') {\n\
		return src;\n\
	}\n\
   /* DLL check.  DLL init function is platform specific.\n\
	*\n\
	* The DLL loader could also need e.g. 'require' to load further modules,\n\
	* but typically native modules don't need to load submodules.\n\
	*/\n\
	name = modHome + id + '.so';\n\
	mod = loadAndInitDll(name, id);\n\
	if (mod) {\n\
		module.exports = mod;  /* replace exports table with module's exports */\n\
		return undefined;\n\
	}\n\
	/* load native module */\n\
	mod = loadNativeModule(modHome, id);\n\
	if (mod) {\n\
		module.exports = mod;\n\
		return undefined;\n\
	}\n\
    /* Must find either a DLL or an ECMAScript file (or both) */\n\
    throw new Error('module not found: ' + id);\n\
}";

#define MAX_PATH_LEN 512
static void set_modSearch(duk_context *ctx, const char *mod_path)
{
	const char *s;
	if (mod_path == NULL) {
		size_t len = MAX_PATH_LEN;
		char *exePath = malloc(len);
		if (exePath == NULL) {
			return;
		}
		getExePath(exePath, len);

		char *exeDir = dirname(exePath);
		s = duk_push_sprintf(ctx, modSearch_impl, exeDir);
		free(exePath);
	} else {
		s = duk_push_sprintf(ctx, modSearch_impl, mod_path);
	}

	int ret = duk_peval(ctx);
	if (ret != 0) {
		s = duk_safe_to_string(ctx, -1);
		fprintf(stderr, "failed to set modSearch: %s\n", s);
	}
	duk_pop(ctx);
}

void* js_create_env(const char *mod_path)
{
	duk_context *ctx = duk_create_heap_default();
	duk_print_alert_init(ctx, 0);
	duk_console_init(ctx, 0);
	duk_module_duktape_init(ctx);
	init_global_funcs(ctx);
	set_modSearch(ctx, mod_path);
	return ctx;
}

void js_destroy_env(void *env)
{
	duk_context *ctx = (duk_context*)env;
	duk_destroy_heap(ctx);
}

int js_register_code_func(void *env, const char *js_code, size_t len, const char *func_name) {
	duk_context *ctx = (duk_context*)env;

	duk_push_global_object(ctx);        // [ global ]
	duk_push_string(ctx, func_name);    // [ global, func_name ]
	if (duk_pcompile_lstring(ctx, DUK_COMPILE_FUNCTION, js_code, len) != 0) {
		fprintf(stderr, "failed to compile script to %s: %s\n", func_name, duk_safe_to_string(ctx, -1));
		duk_pop_3(ctx);
		return -1;
	}
	// [ global, func_name, function ]

	duk_bool_t rc = duk_put_prop(ctx, -3); // [ global ] with global[func_name] = function
	if (rc != 1) {
		fprintf(stderr, "failed to binding script with %s\n", func_name);
		duk_pop(ctx);
		return -2;
	}

	duk_pop(ctx);
	return 0;
}

int js_register_file_func(void *env, const char *script_file, const char *func_name)
{
	duk_context *ctx = (duk_context*)env;
	char *src;
	size_t size;
	int ret = readFileContent(script_file, &src, &size);
	if (ret != 0) {
		return ret;
	}
	
	duk_push_global_object(ctx);        // [ global ]
	duk_push_string(ctx, func_name);    // [ global, func_name ]
	duk_push_string(ctx, script_file);  // [ global, func_name, script_file ]
	if (duk_pcompile_lstring_filename(ctx, DUK_COMPILE_FUNCTION, src, size) != 0) {
		fprintf(stderr, "failed to compile %s: %s\n", script_file, duk_safe_to_string(ctx, -1));
		duk_pop_3(ctx);
		free(src);
		return -6;
	}
	free(src);
	// [ global, func_name, function ]

	duk_bool_t rc = duk_put_prop(ctx, -3); // [ global ] with global[func_name] = function
	if (rc != 1) {
		fprintf(stderr, "failed to binding %s with %s\n", script_file, func_name);
		duk_pop(ctx);
		return -7;
	}

	duk_pop(ctx);
	return 0;
}

int js_unregister_func(void *env, const char *func_name) {
	duk_context *ctx = (duk_context*)env;
	duk_push_global_object(ctx);        // [ global ]
	int rc = duk_del_prop_string(ctx, -1, func_name);
	duk_pop(ctx);
	return rc;
}

static void call_result_callback(duk_context *ctx, fn_call_func_res call_func_res, void *udd) {
	size_t len;
	void *res;
	res_type_t res_type;
	double d;
	switch (duk_get_type(ctx, -1)) {
	case DUK_TYPE_UNDEFINED:
	case DUK_TYPE_NULL:
		res_type = rt_none;
		res = NULL;
		break;
	case DUK_TYPE_BOOLEAN:
		res_type = rt_bool;
		res = (void*)(long)duk_get_boolean(ctx, -1);
		break;
	case DUK_TYPE_NUMBER:
		res_type = rt_double;
		d = duk_get_number(ctx, -1);
		res = double2voidp(d);
		break;
	case DUK_TYPE_STRING:
		res_type = rt_string;
		res = (void*)duk_get_lstring(ctx, -1, &len);
		break;
	case DUK_TYPE_BUFFER:
		res_type = rt_buffer;
		res = (void*)duk_get_buffer(ctx, -1, &len);
		break;
	case DUK_TYPE_OBJECT:
		if (duk_is_ecmascript_function(ctx, -1)) {
			res_type = rt_func;
			// copy the function to the top
			duk_push_null(ctx);   // [ ... null ]
			duk_copy(ctx, -2, -1); // [ ... func ]
			unsigned long func_index = save_top_object(ctx); // [ ... ]
			res = (void*)func_index; // which must be freed by calling js_destropy_ecmascript_func
			break;
		}
	default:
		res_type = duk_is_array(ctx, -1) ? rt_array : rt_object;
		duk_json_encode(ctx, -1);
		res = (void*)duk_get_lstring(ctx, -1, &len);
		break;
	}
	call_func_res(udd, res_type, res, len);
}

static int call_func(duk_context *ctx, int argc, const char *func_name, fn_call_func_res call_func_res, void *udd) {
	// [ func, args ... ]
	duk_int_t rc = duk_pcall(ctx, argc); // [ retval ]
	if (rc != DUK_EXEC_SUCCESS) {
		fprintf(stderr, "failed to run %s() %s\n", func_name, duk_safe_to_string(ctx, -1));
		duk_pop(ctx);
		return -2;
	}

	call_result_callback(ctx, call_func_res, udd);
	duk_pop(ctx);
	return 0;
}

static int push_args_and_call_func(duk_context *ctx, const char *func_name, fn_call_func_res call_func_res, void *udd, char *fmt, void *argv[])
{
	// [ func ]
	if (fmt == NULL || *fmt == '\0') {
		return call_func(ctx, 0, func_name, call_func_res, udd);
	}

	int argc = 0;
	char *s;
	size_t l;
	int d;
	int i = 0;
	unsigned func_index;
	while (*fmt) {
		argc++;
		switch (*fmt++) {
		case af_none:
			i++; //skip it
			duk_push_null(ctx);
			break;
		case af_bool:
			d = (int)(long)argv[i++];
			duk_push_boolean(ctx, d);
			break;
		case af_int:
			d = (int)(long)argv[i++];
			duk_push_int(ctx, d);
			break;
		case af_double:
			duk_push_number(ctx, voidp2double(argv[i++]));
			break;
		case af_zstring:
			s = (char*)argv[i++];
			duk_push_string(ctx, s);
			break;
		case af_lstring:
			l = (size_t)argv[i++];
			s = (char*)argv[i++];
			duk_push_lstring(ctx, s, l);
			break;
		case af_ecmafunc:
			func_index = (unsigned long)argv[i++];
			load_object(ctx, func_index); // now the top ctx is [ func ]
			break;
		case af_jarray:
		case af_jobject:
		default:
			l = (size_t)argv[i++];
			s = (char*)argv[i++];
			duk_push_lstring(ctx, s, l);
			duk_json_decode(ctx, -1);
			break;
		}
	}
	// [ func arg1 arg2 ... argn ]

	return call_func(ctx, argc, func_name, call_func_res, udd);
}

int js_call_registered_func(void *env, const char *func_name, fn_call_func_res call_func_res, void *udd, char *fmt, void *argv[])
{
	duk_context *ctx = (duk_context*)env;
	if (!duk_get_global_string(ctx, func_name)) {
		return -1;
	}
	// [ func ]

	return push_args_and_call_func(ctx, func_name, call_func_res, udd, fmt, argv);
}

int js_call_file_func(void *env, const char *script_file, fn_call_func_res call_func_res, void *udd, char *fmt, void *argv[])
{
	duk_context *ctx = (duk_context*)env;
	char *src;
	size_t size;
	int ret = readFileContent(script_file, &src, &size);
	if (ret != 0) {
		return ret;
	}
	ret = duk_pcompile_lstring(ctx, DUK_COMPILE_FUNCTION, src, size);
	free(src);

	if (ret != 0) {
		fprintf(stderr, "failed to compile %s: %s\n", script_file, duk_safe_to_string(ctx, -1));
		duk_pop(ctx);
		return -1;
	}
	// [ func ]

	return push_args_and_call_func(ctx, script_file, call_func_res, udd, fmt, argv);
}

int js_eval(void *env, const char *js_code, size_t len, fn_call_func_res call_func_res, void *udd)
{
	duk_context *ctx = (duk_context*)env;
	duk_int_t ret = duk_peval_lstring(ctx, js_code, len); // [ retval ]
	if (ret != 0) {
		const char *err = duk_safe_to_string(ctx, -1);
		fprintf(stderr, "error: %s\n", err);
		duk_pop(ctx);
		return -1;
	}
	if (call_func_res != NULL) {
		call_result_callback(ctx, call_func_res, udd);
	}
	duk_pop(ctx);
	return 0;
}

int js_eval_file(void *env, const char *script_file, fn_call_func_res call_func_res, void *udd)
{
	duk_context *ctx = (duk_context*)env;
	char *src;
	size_t size;
	int ret = readFileContent(script_file, &src, &size);
	if (ret != 0) {
		return ret;
	}
	ret = duk_peval_lstring(ctx, src, size);
	free(src);
	if (ret != 0) {
		const char *err = duk_safe_to_string(ctx, -1);
		fprintf(stderr, "error to js_eval_file: %s\n", err);
		duk_pop(ctx);
		return -10;
	}
	if (call_func_res != NULL) {
		call_result_callback(ctx, call_func_res, udd);
	}
	duk_pop(ctx);
	return 0;
}

static duk_ret_t native_func_bridge(duk_context *ctx)
{
	duk_idx_t nargs = duk_get_top(ctx);                           // [ ... ]
	duk_push_current_function(ctx);                               // [ ..., native_func_bridge ]
	duk_get_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL(NATIVE_FUNC)); // [ ..., native_func_bridge, native_func ]
	duk_get_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL(NATIVE_UDD));  // [ ..., native_func_bridge, native_func, udd ]
	fn_native_func native_func = (fn_native_func)duk_get_pointer(ctx, -2);
	void *udd = duk_get_pointer(ctx, -1);	
	duk_pop_3(ctx);               // [ ... ]

	void *cb_res;
	res_type_t res_type;
	size_t res_len;
	fn_free_res free_res = NULL;
	if (nargs == 0) {
		native_func(udd, NULL, NULL, &cb_res, &res_type, &res_len, &free_res);
	} else {
		char *p = (char*)malloc(sizeof(void*) * 2 * nargs + nargs + 1);
		if (p == NULL) {
			return 0;
		}
		void **args = (void**)p;
		char *fmt = p + (sizeof(void*) * 2 * nargs);
		int i, j;
		double d;
		duk_int_t type;
		duk_size_t len;
		for (i=0, j=0; i<nargs; i++) {
			switch (duk_get_type(ctx, i)) {
			case DUK_TYPE_UNDEFINED:
			case DUK_TYPE_NULL:
				fmt[i] = af_none;
				args[j++] = NULL;
				break;
			case DUK_TYPE_BOOLEAN:
				fmt[i] = af_bool;
				args[j++] = (void*)(long)duk_get_boolean(ctx, i);
				break;
			case DUK_TYPE_NUMBER:
				fmt[i] = af_double;
				d = duk_get_number(ctx, i);
				args[j++] = double2voidp(d);
				break;
			case DUK_TYPE_STRING:
				fmt[i] = af_lstring;
				args[j+1] = (void*)duk_get_lstring(ctx, i, &len);
				args[j] = (void*)len;
				j += 2;
				break;
			case DUK_TYPE_BUFFER:
				fmt[i] = af_buffer;
				args[j+1] = (void*)duk_get_buffer(ctx, i, &len);
				args[j] = (void*)len;
				j += 2;
				break;
			case DUK_TYPE_OBJECT:
				if (duk_is_ecmascript_function(ctx, i)) {
					fmt[i] = af_ecmafunc;
					// copy the function to the top
					duk_push_null(ctx);   // [ ... null ]
					duk_copy(ctx, i, -1); // [ ... func ]
					unsigned long func_index = save_top_object(ctx); // [ ... ]
					args[j++] = (void*)func_index; // which must be freed by calling js_destropy_ecmascript_func
					break;
				}
			default:
				fmt[i] = duk_is_array(ctx, i) ? af_jarray : af_jobject;
				duk_json_encode(ctx, i);
				args[j+1] = (void*)duk_get_lstring(ctx, i, &len);
				args[j] = (void*)len;
				j += 2;
				break;
			}
		}

		fmt[nargs] = '\0';
		native_func(udd, fmt, args, &cb_res, &res_type, &res_len, &free_res);
		free(p);
	}

	switch (res_type) {
	case rt_none:
		return 0;
	case rt_bool:
		duk_push_boolean(ctx, (duk_bool_t)(long)cb_res);
		return 1;
	case rt_int:
		duk_push_int(ctx, (duk_int_t)(long)cb_res);
		return 1;
	case rt_double:
		duk_push_number(ctx, (duk_double_t)voidp2double(cb_res));
		return 1;
	case rt_string:
		duk_push_lstring(ctx, (const char*)cb_res, res_len);
		if (free_res != NULL) {
			free_res(cb_res);
		}
		return 1;
	case rt_func:
		load_object(ctx, (unsigned long)cb_res); // now the top of ctx is [ func ]
		return 1;
	case rt_object:
	default:
		duk_push_lstring(ctx, (const char*)cb_res, res_len);
		duk_json_decode(ctx, -1);
		if (free_res != NULL) {
			free_res(cb_res);
		}
		return 1;
	}
}

static void make_func_bridge(duk_context *ctx, const char *func_name, fn_native_func native_func, duk_idx_t nargs, void *udd)
{
	// [ obj ]
	duk_push_string(ctx, func_name);                     // [ obj, func_name ]
	duk_push_c_function(ctx, native_func_bridge, nargs); // [ obj, func_name, native_func_bridge ]
	duk_push_pointer(ctx, native_func);                  // [ obj, func_name, natvie_func_bridge, native_func ]
	duk_put_prop_string(ctx, -2, 
	                    DUK_HIDDEN_SYMBOL(NATIVE_FUNC)); // [ obj, func_name, native_func_bridge ] with native_func_bridge[_nf_] = native_func
	duk_push_pointer(ctx, udd);                          // [ obj, func_name, native_func_bridge, udd ]
	duk_put_prop_string(ctx, -2, 
	                    DUK_HIDDEN_SYMBOL(NATIVE_UDD));  // [ obj, func_name, native_func_bridge ] with native_func_bridge[_nu_] = udd
	duk_put_prop(ctx, -3);                               // [ obj ] with obj[func_name] = native_func_bridge
}

int js_register_native_func(void *env, const char *func_name, fn_native_func native_func, int param_num, void *udd)
{
	duk_context *ctx = (duk_context*)env;
	duk_idx_t nargs;
	if (param_num < 0) {
		nargs = DUK_VARARGS;
	} else {
		nargs = param_num;
	}

	duk_push_global_object(ctx);                               // [ global ]
	make_func_bridge(ctx, func_name, native_func, nargs, udd); // [ global ] with global[func_name] = native_func_bridge
	duk_pop(ctx);
	return 0;
}

int js_unregister_native_func(void *env, const char *func_name)
{
	duk_context *ctx = (duk_context*)env;
	duk_push_global_object(ctx);   // [ global ]
	if (!duk_get_prop_string(ctx, -1, func_name)) {
		duk_pop_2(ctx);
		return -1;
	}
	// [ global, func ]
	if (!duk_get_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL(NATIVE_FUNC))) {
		duk_pop_3(ctx);
		return -2;
	}
	duk_pop_2(ctx); // [ global ]
	duk_del_prop_string(ctx, -1, func_name);
	duk_pop(ctx);
	return 0;
}

int js_call_ecmascript_func(void *env, void *ecma_func, fn_call_func_res call_func_res, void *udd, char *fmt, void *argv[])
{
	duk_context *ctx = (duk_context*)env;
	unsigned long func_index = (unsigned long)ecma_func;
	load_object(ctx, func_index);  // now ctx contains [ func ]
	return push_args_and_call_func(ctx, "_ecmafunc_", call_func_res, udd, fmt, argv);
}

void js_destroy_ecmascript_obj(void *env, void *ecma_obj) {
	duk_context* ctx = (duk_context*)env;
	unsigned long func_index = (unsigned long)ecma_obj;
	destroy_object(ctx, func_index);
}

void* double2voidp(double d) {
	return (void*)*((long*)&d);
}

double voidp2double(void* v) {
	long l = (long)v;
	return *((double*)(&l));
}

void js_add_module_loader(void *env, void *udd, const char *mod_ext, fn_load_module load_module, fn_get_methods_list get_methods_list, fn_get_attrs_list get_attrs_list, fn_module_finalizer finalizer)
{
	duk_context *ctx = (duk_context*)env;
	int loader_count = 0;
	duk_push_global_object(ctx); // [ global ]
	if (duk_get_prop_string(ctx, -1, DUK_HIDDEN_SYMBOL(NATIVE_MOD_COUNT))) {
		loader_count = duk_require_int(ctx, -1);
	}
	duk_pop(ctx);
	duk_push_int(ctx, loader_count+1);
	duk_put_prop_string(ctx, -2, DUK_HIDDEN_SYMBOL(NATIVE_MOD_COUNT));

	duk_push_object(ctx);        // [ global, obj ]

	duk_push_string(ctx, mod_ext);
	duk_put_prop_string(ctx, -2, NATIVE_MOD_EXT);

	duk_push_pointer(ctx, load_module);
	duk_put_prop_string(ctx, -2, NATIVE_MOD_LOADER);

	duk_push_pointer(ctx, get_methods_list);
	duk_put_prop_string(ctx, -2, NATIVE_MOD_METHODS);

	duk_push_pointer(ctx, get_attrs_list);
	duk_put_prop_string(ctx, -2, NATIVE_MOD_ATTRS);

	duk_push_pointer(ctx, finalizer);
	duk_put_prop_string(ctx, -2, NATIVE_MOD_FINALIZER);

	duk_push_pointer(ctx, udd);
	duk_put_prop_string(ctx, -2, NATIVE_UDD);

	char *modNum = getNativeModNum(loader_count);
	duk_put_prop_string(ctx, -2, modNum); // [ global ] with globa[modNum] = obj
	free(modNum);

	duk_pop(ctx);
}

void* js_create_ecmascript_module(void *env, void *udd, void *mod_handle, fn_get_methods_list get_methods_list, fn_get_attrs_list get_attrs_list, fn_module_finalizer finalizer)
{
	duk_context *ctx = (duk_context*)env;
	int ret = init_native_obj(ctx, udd, (const char*)mod_handle, NULL, NULL, NULL, get_methods_list, get_attrs_list, finalizer);
	unsigned long func_index;
	switch (ret) {
	case init_obj_ok:
		func_index = save_top_object(ctx);
		return (void*)func_index;
	case failed_to_init_obj:
	default:
		return NULL;
	}
}
