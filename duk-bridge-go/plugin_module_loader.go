package duk_bridge
/**
 * the default golang plugin module loader
 * Rosbit Xu <me@rosbit.cn>
 * Nov. 10, 2018
 */

import (
	"plugin"
	"fmt"
)

type GoPluginModuleLoader struct{}

const _ext = ".so"

func (loader *GoPluginModuleLoader) SetJSEnv(_ *JSEnv) () {}

func (loader *GoPluginModuleLoader) GetExtName() string {
	return _ext;
}

func (loader *GoPluginModuleLoader) LoadModule(modHome string, modName string) interface{} {
	// fmt.Printf("GoPluginModuleLoader::LoadModule(%s) called\n", modName)
	modPath := fmt.Sprintf("%s/%s%s", modHome, modName, _ext)
	plug, err := plugin.Open(modPath)
	if err != nil {
		return nil
	}

	initFunc, err := plug.Lookup("NewGoModule")
	if err != nil {
		return nil
	}
	structP := initFunc.(func()interface{})()
	return structP
}

func (loader *GoPluginModuleLoader) FinalizeModule(modName string, modHandler interface{}) {
	// fmt.Printf("GoPluginModuleLoader::FinalizeModule(%s) called\n", modName)
}
