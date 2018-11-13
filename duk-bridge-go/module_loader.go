package duk_bridge
/**
 * go module loader interface
 * Rosbit Xu <m@rosbit.cn>
 * Nov. 10, 2018
 */

/*
#include "duk_bridge.h"
#include <stdlib.h>
#include <ctype.h>

extern void go_modBridge(void*, char*, void**, void**, int*, size_t*, fn_free_res*);

extern void* go_loadModule(void*,char*,char*);
extern module_method_t* go_getMethodsList(void*,char*, void*);
extern void go_finalizeModule(void*,char*, void*);
static int mod_entry_size() {
	return sizeof(module_method_t);
}
*/
import "C"

import (
	"unsafe"
	"fmt"
	"reflect"
)

type GoModuleLoader interface {
	GetExtName() string
	LoadModule(modHome string, modName string) interface{}
	FinalizeModule(modName string, modHandler interface{})
}

//export go_modBridge
func go_modBridge(udd unsafe.Pointer, ft *C.char, args *unsafe.Pointer, out_res *unsafe.Pointer, res_type *C.int, res_len *C.size_t, free_res *C.fn_free_res) {
	methodKey := int64(uintptr(udd))
	fun, err := getMethodType(methodKey)
	if err != nil {
		*res_type = C.rt_none
		*out_res = unsafe.Pointer(uintptr(0))
		return
	}
	callGoFunc(*fun, ft, args, out_res, res_type, res_len, free_res)
}

//export go_loadModule
func go_loadModule(udd unsafe.Pointer, modHome *C.char, modName *C.char) unsafe.Pointer {
	loaderKey := int64(uintptr(udd))
	loader, _, err := getModuleLoader(loaderKey)
	if err != nil {
		fmt.Printf("error: %v\n", err)
		return unsafe.Pointer(nil)
	}

	goModName := C.GoString(modName)
	structPtr := loader.LoadModule(C.GoString(modHome), goModName)
	if structPtr == nil {
		return unsafe.Pointer(nil)
	}

	structP := reflect.ValueOf(structPtr)
	if structP.Kind() != reflect.Struct {
		if structP.Kind() != reflect.Ptr {
			fmt.Printf("pointer expected\n")
			return unsafe.Pointer(nil)
		}
		if structP.Elem().Kind() != reflect.Struct {
			fmt.Printf("struct pointer expected\n")
			return unsafe.Pointer(nil)
		}
		// structP = structP.Elem() // strucP is not pointer in such case
	}

	modKey, methodKeys := createMethodKeys(structPtr, structP)
	if methodKeys == nil {
		fmt.Printf("no methods found when initing %s\n", goModName)
		return unsafe.Pointer(nil)
	}

	return unsafe.Pointer(uintptr(modKey))
}

//export go_getMethodsList
func go_getMethodsList(udd unsafe.Pointer, modName *C.char, modHandle unsafe.Pointer) *C.module_method_t {
	modKey := int64(uintptr(modHandle))
	modInfo := getModInfo(modKey)
	if modInfo == nil {
		return (*C.module_method_t)(nil)
	}

	c_method := (*C.module_method_t)(C.malloc(C.size_t(int(C.mod_entry_size()))))
	if c_method == nil {
		fmt.Printf("failed to allocate memory for module_method_t")
		return (*C.module_method_t)(nil)
	}
	defer C.free(unsafe.Pointer(c_method))

	loaderKey := int64(uintptr(udd))
	_, env, _ := getModuleLoader(loaderKey)
	structT := modInfo.structP.Type()
	methodKeys := modInfo.methodKeys
	for i:=0; i<modInfo.nMethods; i++ {
		methodF := modInfo.structP.Method(i)
		funType := methodF.Type()
		methodV := structT.Method(i)

		c_method.name = C.CString(methodV.Name)
		*(c_method.name) = C.char(C.tolower(C.int(*(c_method.name))))
		defer C.free(unsafe.Pointer(c_method.name))

		c_method.method = (*[0]byte)(C.go_modBridge)
		if funType.IsVariadic() {
			c_method.nargs = C.int(-1)
		} else {
			c_method.nargs = C.int(funType.NumIn())
		}
		c_method.udd = unsafe.Pointer(uintptr(methodKeys[i]))
		C.js_add_module_method(env, c_method)
	}
	return (*C.module_method_t)(nil)
}

//export go_finalizeModule
func go_finalizeModule(udd unsafe.Pointer, modName *C.char, modHandle unsafe.Pointer) {
	modKey := int64(uintptr(modHandle))
	modInfo := getModInfo(modKey)

	loaderKey := int64(uintptr(udd))
	loader, _, err := getModuleLoader(loaderKey)
	if err == nil {
		loader.FinalizeModule(C.GoString(modName), modInfo.structPtr)
	}

	removeModule(modKey)
}

func (ctx *JSEnv) addGoModuleLoader(loader GoModuleLoader) {
	if loader == nil {
		return
	}
	ctx.loaderKey = saveModuleLoader(loader, ctx.env)

	ext := loader.GetExtName()
	s := C.CString(ext)
	defer C.free(unsafe.Pointer(s))

	C.js_add_module_loader(ctx.env, unsafe.Pointer(uintptr(ctx.loaderKey)), s, (*[0]byte)(C.go_loadModule), (*[0]byte)(C.go_getMethodsList), (*[0]byte)(C.go_finalizeModule))
}

