CC = gcc

JAVA_INC = /usr/lib/jvm/java-8-openjdk/include
INCS = -I.. -I../duktape -I$(JAVA_INC) -I$(JAVA_INC)/linux

SOURCES = J2CHelper.java \
		 NativeFunc.java \
		 ObjArg.java \
		 FileReader.java \
		 NativeModuleLoader.java \
		 DukBridge.java \
		 JSTest.java

CLASSES = $(subst .java,.class,$(SOURCES))

DUKTAPE_OBJS = duktape.o \
               duk_print_alert.o \
               duk_console.o \
               duk_module_duktape.o

OBJS = $(addprefix ../duktape/, $(DUKTAPE_OBJS))

.SUFFIXES:
.SUFFIXES: .o .c .h .class .java

all: libdukjs.so dukbridge.jar

libdukjs.so: dukbridge.o ../duk_bridge.o $(OBJS)
	$(CC) -shared -o $@ dukbridge.o ../duk_bridge.o $(OBJS) -lm

dukbridge.jar: $(CLASSES) NormalizedArgs.class
	jar cfe $@ JSTest $^

.c.o:
	$(CC) -fPIC -c $< $(INCS)

.java.class:
	javac $<

clean:
	rm -f libdukjs.so dukbridge.jar *.o *.class
