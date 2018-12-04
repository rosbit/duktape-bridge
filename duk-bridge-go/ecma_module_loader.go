package duk_bridge
/**
 * the module loader to create a module as an ecmascript module
 * Rosbit Xu <me@rosbit.cn>
 * Dec. 3, 2018
 */

import (
	"unsafe"
)

type EcmaModuleLoader struct{}

func (loader *EcmaModuleLoader) GetExtName() string {
	return ".every_thing_is_ok";
}

func (loader *EcmaModuleLoader) LoadModule(modHome string, modName string) interface{} {
	return nil
}

func (loader *EcmaModuleLoader) FinalizeModule(modName string, modHandler interface{}) {
}

func (loader *EcmaModuleLoader) saveModule(module unsafe.Pointer) int64 {
	return saveModuleLoader(loader, module)
}
