#include <stdio.h>
#include "duk_bridge.h"
#include <string.h>
#include <stdlib.h>

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
	*res_len  = strlen(r);
	*free_res = free_json;
	*res = r;
}

static module_method_t methods[] = {
	{"adder", adder, 2, NULL},
	{"toJson", toJson, -1, NULL},
	{NULL}
};

static void* load_module(void *udd, const char *mod_home, const char *mod_name) {
	printf("load_module %s called\n", mod_name);
	return (void*)1;
}

static module_method_t* get_methods(void *udd, const char *mod_name, void *mod_handle)
{
	printf("get_methods %s called\n", mod_name);
	return methods;
}

static void finalizer(void *udd, const char *mod_name, void *mod_handle)
{
	printf("finalizer %s called\n", mod_name);
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <script_file> ...\n", argv[0]);
		return 1;
	}
	void *env = js_create_env(NULL);
	js_add_module_loader(env, NULL, ".test", load_module, get_methods, finalizer);

	int i;
	for (i=1; i<argc; i++ ) {
		fprintf(stderr, "running %s...\n", argv[i]);
		if (js_eval_file(env, argv[i], NULL, NULL) != 0) {
			break;
		}
	}

	js_destroy_env(env);
	return 0;
}
