package main

import (
	js "github.com/rosbit/duktape-bridge/duk-bridge-go"
	"os"
	"fmt"
	"encoding/json"
)

var (
	jsEnv *js.JSEnv
)

func adder(a1, a2 float64) float64 {
	return a1 + a2
}

func toJson(args ...interface{}) interface{} {
	if args == nil {
		return nil
	}
	c := len(args)
	if c == 0 {
		return "nothing"
	}
	m := make(map[string]interface{})
	for i:=0; i<c; i++ {
		if args[i] == nil {
			m[fmt.Sprintf("v%d", i)] = nil
			continue
		}
		m[fmt.Sprintf("v%d", i)] = args[i]
	}
	return m
}

var (
	m *js.EcmaObject
)

// go function with a js function type as a argument
func jsCallback(jsFunc *js.EcmaObject) *js.EcmaObject {
	defer jsEnv.DestoryEcmascriptFunc(jsFunc)

	res := jsEnv.CallEcmascriptFunc(jsFunc, "string from duk-bridge-go")
	handleCallFuncResult(res)

	return m
}

type testDukModule struct {}

func (m *testDukModule) Adder(a1, a2 float64) float64 {
	return adder(a1, a2)
}

func (m *testDukModule) ToJson(args ...interface{}) interface{} {
	return toJson(args...)
}

type testModuleLoader struct {}
func (loader *testModuleLoader) GetExtName() string {
	return ".go.so"
}
func (loader *testModuleLoader) LoadModule(modHome string, modName string) interface{} {
	fmt.Printf("LoadModule(%s, %s) in testModuleLoader called\n", modHome, modName)
	return &testDukModule{}
}
func (loader *testModuleLoader) FinalizeModule(modName string, modHandler interface{}) {
	fmt.Printf("FinalizeModule(%s) in testModuleLoader called\n", modName)
}

func handleCallFuncResult(res interface{}) {
	if res == nil {
		fmt.Printf("no result\n")
	} else {
		switch res.(type) {
		case []byte:
			fmt.Printf("result: (bytes)%s\n", string(res.([]byte)))
		case []interface{}:
			if b, err := json.Marshal(res); err == nil {
				fmt.Printf("result: (array)%s\n", string(b))
			} else {
				fmt.Printf("result: (array)%v\n", res)
			}
		case map[string]interface{}:
			if b, err := json.Marshal(res); err == nil {
				fmt.Printf("result: (object)%s\n", string(b))
			} else {
				fmt.Printf("result: (object)%v\n", res)
			}
		default:
			fmt.Printf("result: %v\n", res)
		}
	}
}

func main() {
	if len(os.Args) < 3 {
		fmt.Printf("Usage: %s -{run|jsfunc|jsfile|gofunc|gomodule} <js-file> ...\n", os.Args[0])
		return
	}

	switch os.Args[1] {
	case "-run":
	case "-jsfunc":
	case "-jsfile":
	case "-gofunc":
	case "-gomodule":
	default:
		fmt.Printf("Unknown option %s\n", os.Args[1])
		return
	}
	jsEnv = js.NewEnv(&testModuleLoader{})
	// jsEnv = js.NewEnv(nil)

	argc := len(os.Args)
	switch os.Args[1] {
	case "-run":
		for i:=2; i<argc; i++ {
			fmt.Printf("--------------- run %s ----------------\n", os.Args[i])
			if res, err := jsEnv.EvalFile(os.Args[i]); err != nil {
				fmt.Printf("failed to run: %v\n", err)
			} else {
				handleCallFuncResult(res)
			}
		}
	case "-jsfunc":
		for i:=2; i<argc; i++ {
			fmt.Printf("--------------- run js %s as global func ----------------\n", os.Args[i])
			err := jsEnv.RegisterFileFunc(os.Args[i], "test")
			if err != nil {
				fmt.Printf("failed to register %s: %v\n", os.Args[i], err)
				continue
			}
			// res := jsEnv.CallFunc("test", map[string]interface{}{"Hello":1, "name":"haha"})
			// res := jsEnv.CallFunc("test", fmt.Sprintf("%s %s", "hello", "test"), 1.8)
			// res := jsEnv.CallFunc("test", "hello", "test", 1.8)
			res := jsEnv.CallFunc("test", "hello", "test", 1.8, []int{1, 3})
			handleCallFuncResult(res)
			jsEnv.UnregisterFunc("test")
		}
	case "-jsfile":
		for i:=2; i<argc; i++ {
			fmt.Printf("--------------- run func in script %s ----------------\n", os.Args[i])
			// res := jsEnv.CallFunc("test", map[string]interface{}{"Hello":1, "name":"haha"})
			// res := jsEnv.CallFunc("test", fmt.Sprintf("%s %s", "hello", "test"), 1.8)
			// res := jsEnv.CallFunc("test", "hello", "test", 1.8)
			res := jsEnv.CallFileFunc(os.Args[i], "hello", "test", 1.8, []int{1, 3})
			handleCallFuncResult(res)
		}
	case "-gofunc":
		err := jsEnv.RegisterGoFunc("toJson", toJson)
		if err != nil {
			fmt.Printf("failed to register: %v\n", err)
			break
		}
		jsEnv.RegisterGoFunc("adder", adder)
		jsEnv.RegisterGoFunc("jsCallback", jsCallback)
		m, _ = jsEnv.CreateEcmascriptModule(&testDukModule{})

		for i:=2; i<argc; i++ {
			fmt.Printf("----------- gofunc() ----------\n")
			if res, err := jsEnv.EvalFile(os.Args[i]); err != nil {
				fmt.Printf("failed to run: %v\n", err)
			} else {
				handleCallFuncResult(res)
			}
		}
		jsEnv.UnregisterGoFunc("toJson")
		jsEnv.UnregisterGoFunc("adder")
		jsEnv.UnregisterGoFunc("jsCallback")
	case "-gomodule":
		for i:=2; i<argc; i++ {
			fmt.Printf("----------- gomodule() ----------\n")
			if res, err := jsEnv.EvalFile(os.Args[i]); err != nil {
				fmt.Printf("failed to run: %v\n", err)
			} else {
				handleCallFuncResult(res)
			}
		}
		jsEnv.DesctoryEcmascriptModule(m)
	}
	jsEnv.Destroy()
}
