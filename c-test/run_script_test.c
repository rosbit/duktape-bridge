#include <stdio.h>
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
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <script_file> ...\n", argv[0]);
		return 1;
	}
	void *env = js_create_env(NULL);
	int ret;
	int i;
	for (i=1; i<argc; i++ ) {
		fprintf(stderr, "running %s...\n", argv[i]);
		ret = js_eval_file(env, argv[i], func_res, NULL);
		if (ret != 0) {
			break;
		}
	}
EXIT:
	js_destroy_env(env);
	return ret;
}
