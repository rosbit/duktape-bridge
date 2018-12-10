#include <stdio.h>
#include "duk_bridge.h"
#include <string.h>
#include <stdlib.h>

static void *env = NULL;

static void adder(void* udd, const char* fmt, void *args[], void **res, res_type_t *res_type, size_t *res_len, fn_free_res *free_res)
{
	if (fmt == NULL || fmt[0] == '\0' || fmt[1] == '\0') {
		*res_type = rt_none;
		*res = (void*)NULL;
		return;
	}
	int i;
	double d[2];
	for (i=0; i<2; i++) {
		switch(fmt[i]) {
		case af_double:
			d[i] = voidp2double(args[i]);
			break;
		default:
			*res_type = rt_none;
			*res = (void*)NULL;
			return;
		}
	}
	double r = d[0] + d[1];
	*res_type = rt_double;
	*res = double2voidp(r);
}

static void free_json(void *res) {
	free(res);
}

static void toJson(void* udd, const char* fmt, void *args[], void **res, res_type_t *res_type, size_t *res_len, fn_free_res *free_res)
{
	*res_type = rt_object;
	if (fmt == NULL || fmt[0] == '\0') {
		*res = malloc(20);
		strcpy((char*)res, "empty string");
		*res_len  = strlen((char*)res);
		*free_res = free_json;
		return;
	}

	int count = strlen(fmt);
	char *r = malloc(count * 60);
	int len = 0, i, j;
	
	len += sprintf(r, "{");
	for (i=0, j=0; i<count; i++) {
		len += sprintf(r+len, "\"v%d\":", i);
		switch (fmt[i]) {
		case af_none:
			len += sprintf(r+len, "%s,", "null");
			j++;
			break;
		case af_bool:
			len += sprintf(r+len, "%s,", ((long)args[j]) ? "true": "false");
			j++;
			break;
		case af_double:
			len += sprintf(r+len, "%f,", voidp2double(args[j]));
			j++;
			break;
		case af_lstring:
			len += sprintf(r+len, "\"%.*s\",", (int)(long)args[j], (const char*)args[j+1]);
			j += 2;
			break;
		case af_buffer:
			len += sprintf(r+len, "\"%.*s\",", (int)(long)args[j], (const char*)args[j+1]);
			j += 2;
			break;
		default:
			break;
		}
	}
	if (len > 1) {
		r[len-1] = '}';
	} else {
		r[len++] = '}';
	}
	r[len] = '\0'; 
	*res_len  = len;
	*free_res = free_json;
	*res = r;
}

// --- js as a callback
static void func_res(void* udd, res_type_t res_type, void* res, size_t res_len) {
	switch (res_type) {
	case rt_none:
		printf("null\n");
		break;
	case rt_bool:
		printf("%s\n", (long)res ? "true" : "false");
		break;
	case rt_double:
		printf("%f\n", voidp2double(res));
		break;
	case rt_string:
	case rt_buffer:
	case rt_array:
	case rt_object:
	default:
		printf("%.*s\n", (int)res_len, (char*)res);
		break;
	}
}

static module_method_t methods[] = {
	{"adder", adder, 2, NULL},
	{"toJson", toJson, -1, NULL},
	{NULL}
};
static module_attr_t attrs[] = {
	{"name", af_zstring, (void*)"rosbit", 0},
	{"age", af_int, (void*)(long)20, 0},
	{NULL}
};

static module_method_t* get_methods(void *udd, const char *mod_name, void *mod_handle)
{
	printf("get_methods %s called\n", mod_name);
	return methods;
}

static module_attr_t* get_attrs(void *udd, const char *mod_name, void *mod_handle)
{
	printf("get_attrs %s called\n", mod_name);
	return attrs;
}

static void finalizer(void *udd, const char *mod_name, void *mod_handle)
{
	printf("finalizer %s called\n", mod_name);
}

static void jsCallback(void* udd, const char* fmt, void *args[], void **res, res_type_t *res_type, size_t *res_len, fn_free_res *free_res)
{
	if (fmt == NULL || *fmt == '\0' || fmt[0] != af_ecmafunc) {
		*res_type = rt_object;
		*res = malloc(20);
		strcpy((char*)res, "empty string");
		*res_len  = strlen((char*)res);
		*free_res = free_json;
		return;
	}
	
	void *ecmafunc = args[0]; // if ecmafunc was saved, it could be called times and times again
	// call js func
	void *m = js_create_ecmascript_module(env, NULL, NULL, get_methods, get_attrs, finalizer);

	//void* a = (void*)100;
	//js_call_ecmascript_func(udd, ecmafunc, func_res, NULL, "i", &a);
	js_call_ecmascript_func(udd, ecmafunc, func_res, NULL, "F", &m);
	js_destroy_ecmascript_func(env, ecmafunc);
	js_destroy_ecmascript_module(env, m);

	*res_type = rt_int;
	*res = (void*)(long)10000;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <script_file> ...\n", argv[0]);
		return 1;
	}
	env = js_create_env(NULL);
	js_register_native_func(env, "adder", adder, 2, NULL);
	js_register_native_func(env, "toJson", toJson, -1, NULL);
	js_register_native_func(env, "jsCallback", jsCallback, 1, env);

	int ret;
	int i;
	for (i=1; i<argc; i++ ) {
		fprintf(stderr, "running %s...\n", argv[i]);
		ret = js_eval_file(env, argv[i], NULL, NULL);
		if (ret != 0) {
			break;
		}
	}
EXIT:
	js_destroy_env(env);
	return ret;
}
