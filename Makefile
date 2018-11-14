CC = gcc

DUKTAPE_OBJS = duktape.o \
               duk_print_alert.o \
               duk_console.o \
               duk_module_duktape.o

OBJS = $(addprefix duktape/, $(DUKTAPE_OBJS))

.SUFFIXES:
.SUFFIXES: .o .c .h

all: duk_bridge.so

duk_bridge.so: duk_bridge.o $(OBJS)
	$(CC) -shared -o $@ duk_bridge.o $(OBJS) -ldl

duk_bridge.o: duk_bridge.c duk_bridge.h

.c.o:
	$(CC) -fPIC -o $@ -c $< -Iduktape

clean:
	rm -f $(OBJS) duk_bridge.o duk_bridge.so
