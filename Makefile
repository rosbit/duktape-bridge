CC = gcc

DUKTAPE_OBJS = duktape.o \
               duk_print_alert.o \
               duk_console.o \
               duk_module_duktape.o

.SUFFIXES:
.SUFFIXES: .o .c .h

all: duk_bridge.so

duk_bridge.so: duk_bridge.o $(DUKTAPE_OBJS)
	$(CC) -shared -o $@ duk_bridge.o $(DUKTAPE_OBJS) -ldl

duktape.o: duktape.c duktape.h
duk_print_alert.o: duk_print_alert.c duk_print_alert.h
duk_console.o: duk_console.c duk_console.h
duk_module_duktape.o: duk_module_duktape.c duk_module_duktape.h
duk_bridge.o: duk_bridge.c duk_bridge.h

.c.o:
	$(CC) -fPIC -c $<

clean:
	rm -f $(DUKTAPE_OBJS) duk_bridge.o duk_bridge.so
