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
extern module_method_t* go_getMethodsList(void*, char*, void*);
extern module_attr_t* go_getAttrsList(void*, char*, void*);
extern void go_finalizeModule(void*, char*, void*);
static int mod_method_size() {
	return sizeof(module_method_t);
}
static int mod_attr_size() {
	return sizeof(module_attr_t);
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

	modKey, _ := createMethodKeys(structPtr, structP)
	/*
	modKey, methodKeys := createMethodKeys(structPtr, structP)
	if methodKeys == nil {
		fmt.Printf("no methods found when initing %s\n", goModName)
		return unsafe.Pointer(nil)
	}
	*/

	return unsafe.Pointer(uintptr(modKey))
}

//export go_getMethodsList
func go_getMethodsList(udd unsafe.Pointer, modName *C.char, modHandle unsafe.Pointer) *C.module_method_t {
	modKey := int64(uintptr(modHandle))
	modInfo := getModInfo(modKey)
	if modInfo == nil {
		return (*C.module_method_t)(nil)
	}

	c_method := (*C.module_method_t)(C.malloc(C.size_t(int(C.mod_method_size()))))
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

//export go_getAttrsList
func go_getAttrsList(udd unsafe.Pointer, modName *C.char, modHandle unsafe.Pointer) *C.module_attr_t {
	modKey := int64(uintptr(modHandle))
	modInfo := getModInfo(modKey)
	if modInfo == nil {
		return (*C.module_attr_t)(nil)
	}

	c_attr := (*C.module_attr_t)(C.malloc(C.size_t(int(C.mod_attr_size()))))
	if c_attr == nil {
		fmt.Printf("failed to allocate memory for module_attr_t")
		return (*C.module_attr_t)(nil)
	}
	defer C.free(unsafe.Pointer(c_attr))

	loaderKey := int64(uintptr(udd))
	_, env, _ := getModuleLoader(loaderKey)
	structP := modInfo.structP.Elem()
	nFields := structP.NumField()
	structT := structP.Type()
	for i:=0; i<nFields; i++ {
		field := structP.Field(i)
		fieldV := structT.Field(i)

		firstLetter := fieldV.Name[0]
		if firstLetter == '_' || (firstLetter >= 'a' && firstLetter <= 'z') {
			continue
		}

		c_attr.name = C.CString(fieldV.Name)
		*(c_attr.name) = C.char(C.tolower(C.int(*(c_attr.name))))
		defer C.free(unsafe.Pointer(c_attr.name))

		var cs *C.char
		var l C.int
		switch (field.Kind()) {
		case reflect.Bool:
			c_attr.fmt = C.af_bool
			if field.Bool() {
				c_attr.val = unsafe.Pointer(uintptr(1))
			} else {
				c_attr.val = unsafe.Pointer(uintptr(0))
			}
		case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32:
			c_attr.fmt = C.af_int
			c_attr.val = unsafe.Pointer(uintptr(field.Int()))
		case reflect.Int64:
			c_attr.fmt = C.af_double
			c_attr.val = C.double2voidp(C.double(field.Int()))
		case reflect.Uint8, reflect.Uint16:
			c_attr.fmt = C.af_int
			c_attr.val = unsafe.Pointer(uintptr(field.Uint()))
		case reflect.Uint, reflect.Uint32, reflect.Uint64:
			c_attr.fmt = C.af_double
			c_attr.val = C.double2voidp(C.double(field.Uint()))
		case reflect.Float32, reflect.Float64:
			c_attr.fmt = C.af_double
			c_attr.val = C.double2voidp(C.double(field.Float()))
		case reflect.String:
			c_attr.fmt = C.af_lstring
			s := field.String()
			getStrPtrLen(&s, &cs, &l)
			c_attr.val = unsafe.Pointer(cs)
			c_attr.val_len = C.size_t(l)
		case reflect.Slice:
			if field.IsNil() {
				c_attr.fmt = C.af_none
				break
			}
			fallthrough
		case reflect.Array:
			c_attr.fmt = C.af_jarray
			t := field.Type()
			if t.Elem().Kind() == reflect.Uint8 {
				c_attr.fmt = C.af_lstring
				s := field.Bytes()
				getBytesPtrLen(s, &cs, &l)
				c_attr.val = unsafe.Pointer(cs)
				c_attr.val_len = C.size_t(l)
			} else {
				argToJson(field.Interface(), &cs, &l)
				c_attr.val = unsafe.Pointer(cs)
				c_attr.val_len = C.size_t(l)
			}
		case reflect.Map:
			if field.IsNil() {
				c_attr.fmt = C.af_none
			} else {
				c_attr.fmt = C.af_jobject
				argToJson(field.Interface(), &cs, &l)
				c_attr.val = unsafe.Pointer(cs)
				c_attr.val_len = C.size_t(l)
			}
		default:
			continue
		}

		C.js_add_module_attr(env, c_attr)
	}
	return (*C.module_attr_t)(nil)
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
	loaderKey := saveModuleLoader(loader, ctx.env)
	ctx.loaderKey = append(ctx.loaderKey, loaderKey)

	ext := loader.GetExtName()
	s := C.CString(ext)
	defer C.free(unsafe.Pointer(s))

	C.js_add_module_loader(ctx.env, unsafe.Pointer(uintptr(loaderKey)), s, (*[0]byte)(C.go_loadModule), (*[0]byte)(C.go_getMethodsList), (*[0]byte)(C.go_getAttrsList), (*[0]byte)(C.go_finalizeModule))
}

func (ctx *JSEnv) struct2EcmaModule(structPtr interface{}) (*EcmaObject, error) {
	if structPtr == nil {
		return nil, fmt.Errorf("nil struct given")
	}

	structP := reflect.ValueOf(structPtr)
	if structP.Kind() != reflect.Struct {
		if structP.Kind() != reflect.Ptr {
			return nil, fmt.Errorf("pointer expected")
		}
		if structP.Elem().Kind() != reflect.Struct {
			return nil,  fmt.Errorf("struct pointer expected")
		}
		// structP = structP.Elem() // strucP is not pointer in such case
	}

	modKey, _ := createMethodKeys(structPtr, structP)
	/*
	modKey, methodKeys := createMethodKeys(structPtr, structP)
	if methodKeys == nil {
		return nil, fmt.Errorf("no methods found when initing module")
	}
	*/

	loaderKey := ctx.loaderKey[0]
	m := C.js_create_ecmascript_module(ctx.env, unsafe.Pointer(uintptr(loaderKey)), unsafe.Pointer(uintptr(modKey)), (*[0]byte)(C.go_getMethodsList), (*[0]byte)(C.go_getAttrsList), (*[0]byte)(C.go_finalizeModule))

	return wrapEcmaObject(m, false), nil
}

