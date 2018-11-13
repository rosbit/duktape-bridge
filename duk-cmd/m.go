package main

import (
	js "duk-bridge-go"
	"os"
	"fmt"
)

func main() {
	if len(os.Args) == 1 {
		fmt.Printf("Usage: %s <js-file>\n", os.Args[0])
		os.Exit(1)
	}

	jsEnv := js.NewEnv(nil)
	defer jsEnv.Destroy()

	res, err := jsEnv.EvalFile(os.Args[1])
	if err != nil {
		fmt.Printf("failed to run %s: %v\n", os.Args[1], err)
		os.Exit(2)
	}

	if res != nil {
		fmt.Printf("==> %v\n", res)
	} else {
		fmt.Printf("==> undefined\n")
	}
}
