#include <stdio.h>
#include <stdlib.h>
#include "duk_bridge.h"

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

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <js_file> <func> <args>...\n", argv[0]);
		return 1;
	}
	void *env = js_create_env(NULL);
	int ret;
	ret = js_register_file_func(env, argv[1], argv[2]);
	if (ret != 0) {
		fprintf(stderr, "failed to register_func: %d\n", ret);
		goto EXIT;
	}
	fprintf(stderr, "register ok\n");
	int args_count = argc - 3;
	if (args_count == 0) {
		ret = js_call_registered_func(env, argv[2], func_res, NULL, NULL, NULL);
		goto EXIT;
	}
	char *fmt = malloc(args_count + 1);
	int i;
	for (i=0; i<args_count; i++) {
		fmt[i] = af_zstring;
	}
	fmt[i] = '\0';

	ret = js_call_registered_func(env, argv[2], func_res, NULL, fmt, (void**)&argv[3]);
	free(fmt);
EXIT:
	js_destroy_env(env);
	return ret;
}
