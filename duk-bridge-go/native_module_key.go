package duk_bridge
/**
 * utils to maintain a integer to a golang memory, which is forbidden to
 * be transfered to C langusage.
 * Rosbit Xu <me@rosbit.cn>
 * Nov. 12, 2018
 */

import (
	"time"
	"reflect"
	"unsafe"
	"fmt"
)

type goModuleInfo struct {
	structPtr interface{}
	structP reflect.Value
	nMethods int
	methodKeys []int64
}

type moduleLoaderEnv struct {
	loader GoModuleLoader
	env    unsafe.Pointer
}

var (
	methodKeyGenerator *V2KPool
)

func init() {
	keyGenerator := func(interface{}) (interface{}, error) {
		return time.Now().UnixNano(), nil
	}
	methodKeyGenerator = NewV2KPool(keyGenerator, false)
}

func saveModuleLoader(loader GoModuleLoader, env unsafe.Pointer) int64 {
	loaderKey, _ := methodKeyGenerator.V2K(&moduleLoaderEnv{loader, env})
	return loaderKey.(int64)
}

func getModuleLoader(loaderKey int64) (GoModuleLoader, unsafe.Pointer, error) {
	v := methodKeyGenerator.GetVal(loaderKey)
	switch v.(type) {
	case *moduleLoaderEnv:
		le := v.(*moduleLoaderEnv)
		return le.loader, le.env, nil
	default:
		return nil, nil, fmt.Errorf("no such loaderKey: %v", loaderKey)
	}
}

func removeModuleLoader(loaderKey int64) {
	methodKeyGenerator.RemoveKey(loaderKey)
}

func getModInfo(modKey int64) *goModuleInfo {
	goModule := methodKeyGenerator.GetVal(modKey)
	switch goModule.(type) {
	case *goModuleInfo:
		return goModule.(*goModuleInfo)
	default:
		return nil
	}
}

func createMethodKeys(structPtr interface{}, structP reflect.Value) (int64, []int64) {
	var methodKeys []int64

	nMethods := structP.NumMethod()
	if nMethods > 0 {
		methodKeys = make([]int64, nMethods)
		for i:=0; i<nMethods; i++ {
			methodF := structP.Method(i)
			methodKey, _ := methodKeyGenerator.V2K(&methodF)
			methodKeys[i] = methodKey.(int64)
		}
	}

	goModule := &goModuleInfo{structPtr, structP, nMethods, methodKeys}
	modKey, _ := methodKeyGenerator.V2K(goModule)
	return modKey.(int64), methodKeys
}

func getMethodType(methodKey int64) (*reflect.Value, error) {
	v := methodKeyGenerator.GetVal(methodKey)
	switch v.(type) {
	case *reflect.Value:
		return v.(*reflect.Value), nil
	default:
		return nil, fmt.Errorf("no such methodKey: %v", methodKey)
	}
}

func removeModule(modKey int64) {
	v := methodKeyGenerator.GetVal(modKey)
	switch v.(type) {
	case *goModuleInfo:
		break
	default:
		return
	}

	modInfo := v.(*goModuleInfo)
	methodKeyGenerator.RemoveKey(modKey)

	for i:=0; i<modInfo.nMethods; i++ {
		methodKeyGenerator.RemoveKey(modInfo.methodKeys[i])
	}
}
