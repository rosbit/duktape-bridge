package duk_bridge
/**
 * a wrapper of Duktape JS engine with cgo
 * Rosbit Xu <me@rosbit.cn>
 * Oct. 30, 2018
 */

/*
#cgo CFLAGS: -I.. -I../duktape
#cgo LDFLAGS: -ldl -lm
#include "duk_bridge.h"
#include <string.h>
#include <stdlib.h>
extern void go_resultReceived(void*, int, void*, size_t);
extern void go_funcBridge(void*, char*, void**, void**, int*, size_t*, fn_free_res*);
*/
import "C"

import (
	"unsafe"
	"reflect"
	"fmt"
)

/**
 * type of JS environment.
 */
type JSEnv struct {
	env unsafe.Pointer
	loaderKey []int64
	ecmaModuleLoader *EcmaModuleLoader
}

/**
 * create a new JS environment.
 * @param loader  a implementation of go module loader, nil if none.
 * @return a new JSEnv if ok, otherwise nil
 */
func NewEnv(loader GoModuleLoader) *JSEnv {
	jsEnv := &JSEnv{C.js_create_env(nil), make([]int64, 0, 3), &EcmaModuleLoader{}}
	jsEnv.addGoModuleLoader(&GoPluginModuleLoader{})
	jsEnv.addGoModuleLoader(jsEnv.ecmaModuleLoader)
	if loader != nil {
		jsEnv.addGoModuleLoader(loader)
	}
	return jsEnv
}

/**
 * destory a JS environment.
 */
func (ctx *JSEnv) Destroy() {
	C.js_destroy_env(ctx.env)
	if ctx.loaderKey != nil {
		for i:=0; i<len(ctx.loaderKey); i++ {
			if ctx.loaderKey[i] != 0 {
				removeModuleLoader(ctx.loaderKey[i])
			}
		}
	}
}

func fromErrorCode(res C.int) error {
	if res == 0 {
		return nil
	}
	return fmt.Errorf("error code: %d", int(res))
}

/**
 * evaluate any lines of JS codes.
 * @param jsCode  JS syntax satisfied codes.
 * @return nil if ok
 */
func (ctx *JSEnv) Eval(jsCode string) (interface{}, error) {
	var s *C.char
	var l C.int
	getStrPtrLen(&jsCode, &s, &l)

	var res interface{} = nil // pointer to result
	ret := C.js_eval(ctx.env, s, C.size_t(l), (*[0]byte)(C.go_resultReceived), unsafe.Pointer(&res))
	if ret != C.int(0) {
		return nil, fromErrorCode(ret)
	}
	return res, nil
}

/**
 * evaluate any lines of JS codes.
 * @param jsCode  JS syntax satisfied codes.
 * @return nil if ok
 */
func (ctx *JSEnv) EvalBytes(jsCode []byte) (interface{}, error) {
	var s *C.char
	var l C.int
	getBytesPtrLen(jsCode, &s, &l)

	var res interface{} = nil // pointer to result
	ret := C.js_eval(ctx.env, s, C.size_t(l), (*[0]byte)(C.go_resultReceived), unsafe.Pointer(&res))
	if ret != C.int(0) {
		return nil, fromErrorCode(ret)
	}
	return res, nil
}

/**
 * evaluate JS codes in a file.
 * @param scriptFile  the script file
 * @return nil if ok.
 */
func (ctx *JSEnv) EvalFile(scriptFile string) (interface{}, error) {
	f := C.CString(scriptFile)
	defer C.free(unsafe.Pointer(f))

	var res interface{} = nil // pointer to result
	ret := C.js_eval_file(ctx.env, f,  (*[0]byte)(C.go_resultReceived), unsafe.Pointer(&res))
	if ret != C.int(0) {
		return nil, fromErrorCode(ret)
	}
	return res, nil
}

/**
 * register a function in a JS script file. the function could be called by JSEnv::CallFunc() later.
 * @param scriptFile  the script file containing a function
 * @param funcName    the function name to be registered, which can be different from the name in the scriptFile.
 * @return nil if ok.
 */
func (ctx *JSEnv) RegisterFileFunc(scriptFile string, funcName string) error {
	sf := C.CString(scriptFile)
	defer C.free(unsafe.Pointer(sf))
	fn := C.CString(funcName)
	defer C.free(unsafe.Pointer(fn))

	res := C.js_register_file_func(ctx.env, sf, fn)
	return fromErrorCode(res)
}

func (ctx *JSEnv) RegisterCodeFunc(jsCode []byte, funcName string) error {
	fn := C.CString(funcName)
	defer C.free(unsafe.Pointer(fn))

	var js *C.char
	var len C.int
	getBytesPtrLen(jsCode, &js, &len)
	res := C.js_register_code_func(ctx.env, js, C.size_t(len), fn)
	return fromErrorCode(res)
}

/**
 * unregister a function which was registered by calling JSEnv::RegisterFileFunc()/RegisterCodeFunc()
 * @param funcName  the name of function to be unregistered.
 * @return nil if ok.
 */
func (ctx *JSEnv) UnregisterFunc(funcName string) error {
	fn := C.CString(funcName)
	defer C.free(unsafe.Pointer(fn))

	res := C.js_unregister_func(ctx.env, fn)
	return fromErrorCode(res)
}

/**
 * call a JS function registered by JSEnv::RegisterFileFunc()/RegisterCodeFunc()
 * @param funcName  the registered function name when calling JSEnv::RegisterFileFunc()/RegisterCodeFunc()
 * @param args      any count of array of anything
 * @return any type data
 */
func (ctx *JSEnv) CallFunc(funcName string, args ...interface{}) interface{} {
	fn := C.CString(funcName)
	defer C.free(unsafe.Pointer(fn))

	var res interface{} = nil // pointer to result
	if args == nil {
		// no args, call the js function directly which will trigger go_resultReceived()
		C.js_call_registered_func(ctx.env, fn, (*[0]byte)(C.go_resultReceived), unsafe.Pointer(&res), (*C.char)(C.NULL), (*unsafe.Pointer)(unsafe.Pointer(nil)))
	} else {
		// translate the arguments for C.
		_, fmt, argv := parseArgs(args)

		var f *C.char
		getBytesPtr(fmt, &f)  // f -> fmt
		var a *unsafe.Pointer
		getArgsPtr(argv, &a)  // a -> argv
		C.js_call_registered_func(ctx.env, fn, (*[0]byte)(C.go_resultReceived), unsafe.Pointer(&res), f, a)
	}

	return res
}

/**
 * call a JS script file containing only one function
 * @param scriptFile  the JS file with only one function
 * @param args        any count of array of anything
 * @return any type data
 */
func (ctx *JSEnv) CallFileFunc(scriptFile string, args ...interface{}) interface{} {
	fn := C.CString(scriptFile)
	defer C.free(unsafe.Pointer(fn))

	var res interface{} = nil // pointer to result
	if args == nil {
		// no args, call the js function directly which will trigger go_resultReceived()
		C.js_call_file_func(ctx.env, fn, (*[0]byte)(C.go_resultReceived), unsafe.Pointer(&res), (*C.char)(C.NULL), (*unsafe.Pointer)(unsafe.Pointer(nil)))
	} else {
		// translate the arguments for C.
		_, fmt, argv := parseArgs(args)

		var f *C.char
		getBytesPtr(fmt, &f)  // f -> fmt
		var a *unsafe.Pointer
		getArgsPtr(argv, &a)  // a -> argv
		C.js_call_file_func(ctx.env, fn, (*[0]byte)(C.go_resultReceived), unsafe.Pointer(&res), f, a)
	}

	return res
}

/**
 * the bridge func used by JSEnv::RegisterGlobalGoFunc()
 */
//export go_funcBridge
func go_funcBridge(udd unsafe.Pointer, ft *C.char, args *unsafe.Pointer, out_res *unsafe.Pointer, res_type *C.int, res_len *C.size_t, free_res *C.fn_free_res) {
	fn := *((*interface{})(udd))
	fun := reflect.ValueOf(fn)

	callGoFunc(fun, ft, args, out_res, res_type, res_len, free_res)
}

/**
 * register a global native function witten in golang. so JS code will call it later.
 * @param funcName  the function name to be registered
 * @param fn        the golang function to response the JS calling.
 * @return nil if ok.
 */
func (ctx *JSEnv) RegisterGoFunc(funcName string, fn interface{}) error {
	fun := reflect.ValueOf(fn)
	if fun.Kind() != reflect.Func {
		return fmt.Errorf("go function expected")
	}
	funType := fun.Type()
	var nargs int
	if funType.IsVariadic() {
		nargs = -1
	} else {
		nargs = funType.NumIn()
	}

	funcN := C.CString(funcName)
	defer C.free(unsafe.Pointer(funcN))
	res := C.js_register_native_func(ctx.env, funcN, ((*[0]byte))(C.go_funcBridge), C.int(nargs), unsafe.Pointer(&fn))
	return fromErrorCode(res)
}

func (ctx *JSEnv) UnregisterGoFunc(funcName string) error {
	funcN := C.CString(funcName)
	defer C.free(unsafe.Pointer(funcN))
	res := C.js_unregister_native_func(ctx.env, funcN)
	return fromErrorCode(res)
}

func (ctx *JSEnv) CallEcmascriptFunc(ecmaFunc *EcmaObject, args ...interface{}) interface{} {
	var res interface{} = nil // pointer to result
	if args == nil {
		// no args, call the js function directly which will trigger go_resultReceived()
		C.js_call_ecmascript_func(ctx.env, ecmaFunc.ecmaObj, (*[0]byte)(C.go_resultReceived), unsafe.Pointer(&res), (*C.char)(C.NULL), (*unsafe.Pointer)(unsafe.Pointer(nil)))
	} else {
		// translate the arguments for C.
		_, fmt, argv := parseArgs(args)

		var f *C.char
		getBytesPtr(fmt, &f)  // f -> fmt
		var a *unsafe.Pointer
		getArgsPtr(argv, &a)  // a -> argv
		C.js_call_ecmascript_func(ctx.env, ecmaFunc.ecmaObj, (*[0]byte)(C.go_resultReceived), unsafe.Pointer(&res), f, a)
	}

	return res
}

func (ctx *JSEnv) DestoryEcmascriptFunc(ecmaFunc *EcmaObject) {
	ecmaFunc.Destroy()
}

func (ctx *JSEnv) CreateEcmascriptModule(structPtr interface{}) (*EcmaObject, error) {
	return ctx.struct2EcmaModule(structPtr)
}

func (ctx *JSEnv) DesctoryEcmascriptModule(module *EcmaObject) {
	module.Destroy()
}
