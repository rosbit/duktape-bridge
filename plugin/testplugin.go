package main

import (
	"fmt"
)

type TestJsModule struct
{
	Name string
	Age int
	Other map[string]interface{}
}

func NewGoModule() interface{} {
	return &TestJsModule{"rosbit", 20, map[string]interface{}{"i":1, "s":"string"}}
}

func (mod* TestJsModule) Adder(a1, a2 float64) float64 {
	return a1 + a2
}

func (mod* TestJsModule) ToJson(args ...interface{}) interface{} {
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
