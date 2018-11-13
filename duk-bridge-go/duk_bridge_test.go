package duk_bridge

import (
	"fmt"
	"encoding/json"
	"testing"
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

var jsEnv *JSEnv

func init() {
	jsEnv = NewEnv(&testModuleLoader{})
}

func Test_eval(t *testing.T) {
	t.Log("------------- run js/r.js -------------\n")
	if res, err := jsEnv.EvalFile("js/r.js"); err != nil {
		t.Errorf("failed to run: %v\n", err)
	} else {
		handleCallFuncResult(res)
	}
}

func Test_jsfunc(t *testing.T) {
	t.Logf("--------------- run js test/a.js as global func ----------------\n")
	err := jsEnv.RegisterFileFunc("js/a.js", "test")
	if err != nil {
		t.Errorf("failed to register js/a.js: %v\n", err)
	}
	// res := jsEnv.CallFunc("test", map[string]interface{}{"Hello":1, "name":"haha"})
	// res := jsEnv.CallFunc("test", fmt.Sprintf("%s %s", "hello", "test"), 1.8)
	// res := jsEnv.CallFunc("test", "hello", "test", 1.8)
	res := jsEnv.CallFunc("test", "hello", "test", 1.8, []int{1, 3})
	handleCallFuncResult(res)
	jsEnv.UnregisterFunc("test")
}

func Test_gofunc(t *testing.T) {
	err := jsEnv.RegisterGoFunc("toJson", toJson)
	if err != nil {
		t.Errorf("failed to register: %v\n", err)
		return
	}
	jsEnv.RegisterGoFunc("adder", adder)
	t.Log("----------- gofunc() ----------\n")
	if res, err := jsEnv.EvalFile("js/call_native.js"); err != nil {
		t.Errorf("failed to run: %v\n", err)
	} else {
		handleCallFuncResult(res)
	}
	jsEnv.UnregisterGoFunc("toJson")
	jsEnv.UnregisterGoFunc("adder")
}

func Test_gomodule(t *testing.T) {
	t.Log("----------- gomodule() ----------\n")
	if res, err := jsEnv.EvalFile("js/native_module.js"); err != nil {
		t.Errorf("failed to run: %v\n", err)
	} else {
		handleCallFuncResult(res)
	}
}
