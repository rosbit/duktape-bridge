package main

import (
	"fmt"
)

type TestJsModule struct{}

func NewGoModule() interface{} {
	return &TestJsModule{}
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
