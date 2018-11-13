CC = gcc

.SUFFIXES:
.SUFFIXES: .o .c .h

EXES = run_func_test run_script_test run_call_native_test run_file_func \
	run_native_module_test

all: $(EXES)

.c: ../duk_bridge.so
	$(CC) -o $@ $< -I.. ../duk_bridge.so -lm

clean:
	rm -f $(EXES)
