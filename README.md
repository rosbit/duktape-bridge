# Duktape bridge for C, Go(Golang) and Java.

[Duktape](http://duktape.org/index.html) is a thin, embeddable javascript engine.
Its [api](http://duktape.org/api.html) is very well documented. For most of developers
who are not familiar with C language, it is very tedious to call duk\_push\_xxx() and
duk\_pop() to make use of the power of Duktape. Though there are some binding
implementations of Duktape for languages other than C, most of them inherit the
methods of using API of Duktape.

This package is intended to implement a wrapper called duk-bridge in C, and on top of
the C bridge, wrappers in Go and Java are implemented. All of the bridge wrappers
provide very common functions just like Eval(), CallFunc(), RegisterGoFunc().
So even without any Duktape knowledge, one can embed Duktape to an application.

What's more, Go functions registered to be called in JS are common Go functions.
Given a logic function written in Go, the Go bridge will convert arguments of
JavaScript code to those of Go function, call the Go function and convert the result
from Go function to JavaScript function.

The Go bridge can also make your Go package to a Duktape JavaScript module. So
what you can do is just writing `var mod = require('your_module')`, your Go package is
ready to be called by JS.

### Usage

The package is fully go-getable, no need to install any external C libraries. 
So, just type

  `go get github.com/rosbit/duktape-bridge/duk-bridge-go`

to install.

```go
package main

import "fmt"
import js "github.com/rosbit/duktape-bridge/duk-bridge-go"

func main() {
  ctx := js.NewEnv(nil)
  defer ctx.Destroy()

  res, _ := ctx.Eval("2 + 3")
  fmt.Println("result is:", res)

  /*
  // or run a js file
  res, _ := ctx.EvalFile("filename.js")
  fmt.Println("result is:", res)
  */
}
```

### Go calls JavaScript function

Suppose there's a js file named `a.js` like this:

```js
function test() {
        var prefix = 'v'
        var res = {}
        for (var i=0; i<arguments.length; i++) {
                res[prefix+i] = arguments[i]
        }
        return res // boolean, integer, number, array, object are acceptable
}
```

one can call the js function test() in Go code like the following:

```go
package main

import "fmt"
import js "github.com/rosbit/duktape-bridge/duk-bridge-go"

func main() {
  ctx := js.NewEnv(nil)
  defer ctx.Destroy()

  res := ctx.CallFileFunc("a.js", "args 1", 2, true, "other args") // only filename is needed.
  fmt.Println("result is:", res)

  /*
  // or if you want to call the js function more than 1 times, just register it first
  ctx.RegisterFileFunc("a.js", "test")
  ctx.CallFunc("test", "args 1", 2, true, "other args")
  ctx.CallFunc("test", "array args", []int{1, 2, 3})
  ctx.CallFunc("test", "map args", map[string]interface{}{"str":"anything", "int": 100})
  ctx.UnregisterFunc("test") // unregister it if you don't use it anymore
  */
}
```

### JavaScript calls Go function

JavaScript calling Go function is also easy. In the Go code, register a function with
a name by calling `RegisterGoFunc("name", function)`. There's the Go example:

```go
package main

import js "github.com/rosbit/duktape-bridge/duk-bridge-go"

// function to be called by JS
func adder(a1 float64, a2 float64) float64 {
	return a1 + a2
}

func main() {
  ctx := js.NewEnv(nil)
  defer ctx.Destroy()

  ctx.RegisterGoFunc("adder", adder)
  // ctx.UnregisterGoFunc("adder")  // unregister it if you don't use it anymore
  ctx.EvalFile("b.js")  // b.js containing code calling "adder"
}
```

In JS code, one can call the registered name directly. There's the example js `b.js`.

```js
var r = adder(1, 100)   // the function "adder" is implemented in Go
console.log(r)
```

#### The limitation of Go function

If a Go function registered to be called by JS, the types of its arguments and result
must meet some satisfications:

 - primitive type bool, int, intXX, uintXX, float32, float64 are acceptable
 - string and []byte are acceptable
 - array slice of bool, int, intXX, uintXX, float32, float64, string are acceptable. e.g. []bool, []int
 - map with string as the type of key is acceptable. e.g. map[string]interface{}

### Go module and module loader

duk-bridge for Go provides a default module loader which will convert a Go plugin package into
a JS module. There's the example `c.js`:

```js
var m = require('test') // this will load the Go plugin test.so
var r = m.adder(1, 300)
console.log(r)

console.log('m.name', m.name)
console.log('m.age', m.age)
```

The Go code calls the js

```go
package main

import js "github.com/rosbit/duktape-bridge/duk-bridge-go"

func main() {
  ctx := js.NewEnv(nil)
  defer ctx.Destroy()

  ctx.EvalFile("c.js")
}
```
Build it to a directory, make a subdirectory `modules` under it like the following:

```
 bin
  +-- go_exe_file
  +-- modules
        +-- test.so
        +-- <other_so_module>.so
        +-- <js_module>.js
```

The Go plugin module implementation:

```go
package main

type Test struct {
	Name string
	Age int
	Other map[string]interface{}
}

func NewGoModule() interface{} {
	return &Test{"rosbit", 20}
}

func (t *Test) Adder(a1 float64, a2 float64) float64 {
	return a1 + a2
}

func (t *Test) OtherFunctions() {
}
```

Save it as `test.go`, then build it to a Go plugin with the command:
`go build -buildmode=plugin test.go`, a file `test.so` is created.
copy it to the `modules` subdirectory. now run the main Go app.

#### Limitation of Go module

Not all Go packages can become js module. There are some limitations:

 - function NewGoModule with prototype `func NewGoModule() interface{}` must be in the module,
   and it returns a pointer to struct.
 - Though as if function NewGoModule() will be called multi-times, in fact Duktape engine will
   cache the required module and it is called **only once** even if you `require` it multi-times.
   So don't declare module related variables in struct. Only read only constants are acceptable.
 - function/field name with the first letter in capital will be exported as moudle method/attribute.
   For example, `Adder` will be exported but `adder` will not. But to refer the `Adder` method,
   please use `adder` as used in `c.js`

### Duktape bridge for C and Java

 - Duktape bridge for C is under the main directory of the project, just run `make`,
   a file `duk_bridge.so` will be created, with `duk_bridge.h` and `duk_bridge.so`,
   one can embed Duktape in any C/C++ project.
 - Duktape bridge for Java is under the subdiretory `duk-bridge-java`, run `make` to
   create `dukbridge.jar` and `libdukjs.so`. Then one can run
 
   `java -jar dukbridge.jar -Djava.library.path=. <file.js> <func_name>`
 
   to hava a test.
 - Of course, with duktape bridge for C, one can implement duktape bridge for
   other language like Python.
 
### Lua vs. Dutakpe

 - Duktape borrows a lot from Lua conceptually. The usage of Duktape API
   is very similar to that of Lua.
 - The difference of Lua and Duktape is the embedding language: Duketape engine supports
   JavaScript, which is now very popular.

### Node.js vs. Duktape

 - Node.js objects the web application develepment, in the area there exists PHP, 
   Java Servlet and Python WSGI.
 - Duktape focuses the embedding area where one want to add dynamic modification without
   rebuild the main application.

### Status

The package is not fully tested, so be careful.

### Contribution

Pull requests are welcome! Also, if you want to discuss something send a pull request with proposal and changes.
__Convention:__ fork the repository and make changes on your fork in a feature branch.
