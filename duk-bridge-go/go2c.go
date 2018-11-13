package duk_bridge
/**
 * utils to convert args/result between golang and c.
 * Rosbit Xu <me@rosbit.cn>
 * Nov. 12, 2018
 */

/*
#include "duk_bridge.h"
#include <string.h>
#include <stdlib.h>
*/
import "C"

import (
	"unsafe"
	"reflect"
	"encoding/json"
)

func getStrPtr(goStr *string, val **C.char) {
    v := (*reflect.StringHeader)(unsafe.Pointer(goStr))
    *val = (*C.char)(unsafe.Pointer(v.Data))
}

func getStrPtrLen(goStr *string, val **C.char, valLen *C.int) {
    v := (*reflect.StringHeader)(unsafe.Pointer(goStr))
    *val = (*C.char)(unsafe.Pointer(v.Data))
    *valLen = C.int(v.Len)
}

func getBytesPtr(goBytes []byte, val **C.char) {
    p := (*reflect.SliceHeader)(unsafe.Pointer(&goBytes))
    *val = (*C.char)(unsafe.Pointer(p.Data))
}

func getBytesPtrLen(goBytes []byte, val **C.char, valLen *C.int) {
    p := (*reflect.SliceHeader)(unsafe.Pointer(&goBytes))
    *val = (*C.char)(unsafe.Pointer(p.Data))
    *valLen = C.int(p.Len)
}

func getArgsPtr(args []uint64, val **unsafe.Pointer) {
	p := (*reflect.SliceHeader)(unsafe.Pointer(&args))
	*val = (*unsafe.Pointer)(unsafe.Pointer(p.Data))
}

func toBytes(chunk *C.char, length int) []byte {
	var b []byte
	bs := (*reflect.SliceHeader)(unsafe.Pointer(&b))
	bs.Data = uintptr(unsafe.Pointer(chunk))
	bs.Len = int(length)
	bs.Cap = int(length)
	return b
}

func toString(chuck *C.char, length int) *string {
	var s string
    v := (*reflect.StringHeader)(unsafe.Pointer(&s))
	v.Data = uintptr(unsafe.Pointer(chuck))
	v.Len = int(length)
	return &s
}

func toPointerArray(args *unsafe.Pointer, length int) []unsafe.Pointer {
	var a []unsafe.Pointer
	as := (*reflect.SliceHeader)(unsafe.Pointer(&a))
	as.Data = uintptr(unsafe.Pointer(args))
	as.Len = int(length)
	as.Cap = int(length)
	return a
}

/*
 * a bridge callback used by JSEnv::CallFunc() to allocate memory for the JS result.
 * @param udd      the variable which was desclared in JSEnv::CallFunc()
 * @param res_type the type of data store in res
 * @param res      the address pointer to the result of JSEnv::CallFunc(), which can be casted to any C type
 * @param res_len  bytes length of content pointed by res
 */
//export go_resultReceived
func go_resultReceived(udd unsafe.Pointer, res_type C.int, res unsafe.Pointer, res_len C.size_t) {
	pRes := (*interface{})(udd)
	switch res_type {
	case C.rt_none:
		*pRes = nil
	case C.rt_bool:
		*pRes = uint64(uintptr(res)) != 0
	case C.rt_double:
		// p := uint64(uintptr(res))
		// *pRes = *(*float64)(unsafe.Pointer(&p))
		*pRes = C.voidp2double(res)
	case C.rt_string:
		var b []byte     // no allocation

		bs := (*reflect.SliceHeader)(unsafe.Pointer(&b))
		bs.Data = uintptr(res)
		bs.Len = int(res_len)
		bs.Cap = int(res_len)

		*pRes = string(b) // copy to string
	case C.rt_buffer:
		b := make([]byte, res_len) // allocate the memory to store the result.
		bs := (*reflect.SliceHeader)(unsafe.Pointer(&b))
		C.memcpy(unsafe.Pointer(bs.Data), unsafe.Pointer(res), res_len)
		*pRes = b
	case C.rt_object:
		fallthrough
	case C.rt_array:
		fallthrough
	default:
		var b []byte     // no allocation

		bs := (*reflect.SliceHeader)(unsafe.Pointer(&b))
		bs.Data = uintptr(res)
		bs.Len = int(res_len)
		bs.Cap = int(res_len)

		if err := json.Unmarshal(b, pRes); err != nil {
			*pRes = err
		}
	}
}

func argToJson(arg interface{}, p **C.char, pLen *C.int) bool {
	if b, err := json.Marshal(arg); err == nil {
		getBytesPtrLen(b, p, pLen)
		return true
	} else {
		return false
	}
}

func parseArgs(args ...interface{}) (nargs int, fmt[]byte, argv []uint64) {
	nargs = len(args)
	fmt = make([]byte, nargs+1)    // char *fmt in C
	fmt[nargs] = byte(0)           // the ending '\0' for cz-string.
	argv = make([]uint64, nargs*2) // void *argv[] in C. (with uint64 type other than unsafe.Pointer)

	j := 0  // subscript index of argv
	var p *C.char
	var pLen C.int
	for i,arg := range args {
		if arg == nil {
			fmt[i] = C.af_none
			argv[j] = uint64(0)
			j += 1
			continue
		}

		switch arg.(type) {
		case bool:
			fmt[i] = C.af_bool
			if arg.(bool) {
				argv[j] = uint64(1)
			} else {
				argv[j] = uint64(0)
			}
			j += 1
		case int:
			fmt[i] = C.af_int
			argv[j] = uint64(arg.(int))
			j += 1
		case float64:
			fmt[i] = C.af_double
			f := arg.(float64)
			p := (*uint64)(unsafe.Pointer(&f))
			argv[j] = *p
			j += 1
		case string:
			fmt[i] = C.af_lstring
			s := arg.(string)
			getStrPtrLen(&s, &p, &pLen)
			argv[j] = uint64(pLen)
			argv[j+1] = uint64(uintptr(unsafe.Pointer(p)))
			j += 2
		case []byte:
			fmt[i] = C.af_lstring
			getBytesPtrLen(arg.([]byte), &p, &pLen)
			argv[j] = uint64(pLen)
			argv[j+1] = uint64(uintptr(unsafe.Pointer(p)))
			j += 2
		case []interface{}:
			if argToJson(arg, &p, &pLen) {
				fmt[i] = C.af_jarray
				argv[j] = uint64(pLen)
				argv[j+1] = uint64(uintptr(unsafe.Pointer(p)))
				j += 2
			} else {
				fmt[i] = C.af_none
				argv[j] = uint64(0)
				j += 1
			}
		case map[string]interface{}:
			if argToJson(arg, &p, &pLen) {
				fmt[i] = C.af_jobject
				argv[j] = uint64(pLen)
				argv[j+1] = uint64(uintptr(unsafe.Pointer(p)))
				j += 2
			} else {
				fmt[i] = C.af_none
				argv[j] = uint64(0)
				j += 1
			}
		default:
			if argToJson(arg, &p, &pLen) {
				fmt[i] = C.af_jobject
				argv[j] = uint64(pLen)
				argv[j+1] = uint64(uintptr(unsafe.Pointer(p)))
				j += 2
			} else {
				fmt[i] = C.af_none
				argv[j] = uint64(0)
				j += 1
			}
		}
	}

	// return nargs, fmt, argv
	return
}

func resToJson(res interface{}, out_res *unsafe.Pointer, res_type *C.int, res_len *C.size_t) {
	b, err := json.Marshal(res)
	if err != nil {
		*res_type = C.rt_none
		*out_res = unsafe.Pointer(uintptr(0))
	} else {
		*res_type = C.rt_object
		var s *C.char
		var l C.int
		getBytesPtrLen(b, &s, &l)
		*res_len = C.size_t(l)
		*out_res = unsafe.Pointer(s)
	}
}

func setBuffer(res interface{}, out_res *unsafe.Pointer, res_type *C.int, res_len *C.size_t) {
	*res_type = C.rt_string
	var cs *C.char
	var cl C.int

	switch res.(type) {
	case string:
		s := res.(string)
		getStrPtrLen(&s, &cs, &cl)
	case []byte:
		b := res.([]byte)
		getBytesPtrLen(b, &cs, &cl)
	}

	*res_len = C.size_t(cl)
	*out_res = unsafe.Pointer(cs)
}

func getFuncArgType(funType reflect.Type, argIndex int, funNargs int) reflect.Type {
	if funType.IsVariadic() {
		if argIndex < funNargs-2 {
			return funType.In(argIndex)
		}
		return funType.In(funNargs-1).Elem()
	}
	return funType.In(argIndex)
}

func callGoFunc(fun reflect.Value, ft *C.char, args *unsafe.Pointer, out_res *unsafe.Pointer, res_type *C.int, res_len *C.size_t, free_res *C.fn_free_res) {
	funType := fun.Type()
	funNargs := funType.NumIn()

	var r []reflect.Value
	if ft == (*C.char)(C.NULL) {
		if funNargs == 0 || (funType.IsVariadic() && funNargs == 1) {
			r = fun.Call(nil)
		} else {
			*res_type = C.rt_none
			*out_res = unsafe.Pointer(uintptr(0))
			return
		}
	} else {
		nargs := int(C.strlen(ft))
		if funType.IsVariadic() {
			if nargs < funNargs-1 {
				*res_type = C.rt_none
				*out_res = unsafe.Pointer(uintptr(0))
				return
			}
		} else {
			if nargs != funNargs {
				*res_type = C.rt_none
				*out_res = unsafe.Pointer(uintptr(0))
				return
			}
		}

		if nargs == 0 {
			r = fun.Call(nil)
		} else {
			bft := toBytes(ft, nargs)
			arrArgs := toPointerArray(args, 2*nargs)
			argv := make([]reflect.Value, nargs)
			j := 0
			for i:=0; i<nargs; i++ {
				funArgType := getFuncArgType(funType, i, funNargs)
				switch bft[i] {
				case C.af_none:
					argv[i] = reflect.Zero(funArgType)
					j += 1
				case C.af_bool:
					argv[i] = reflect.ValueOf(int(uintptr(arrArgs[j])) != 0)
					j += 1
				case C.af_double:
					p := uint64(uintptr(arrArgs[j]))
					argv[i] = reflect.ValueOf(*(*float64)(unsafe.Pointer(&p)))
					j += 1
				case C.af_lstring:
					l := int(uintptr(arrArgs[j]))
					s := (*C.char)(arrArgs[j+1])
					j += 2
					argv[i] = reflect.ValueOf(*(toString(s, l)))
				case C.af_buffer:
					l := int(uintptr(arrArgs[j]))
					s := (*C.char)(arrArgs[j+1])
					j += 2
					argv[i] = reflect.ValueOf(toBytes(s, l))
				case C.af_jobject:
					l := int(uintptr(arrArgs[j]))
					s := (*C.char)(arrArgs[j+1])
					j += 2
					b := toBytes(s, l)
					var o map[string]interface{}
					if json.Unmarshal(b, &o) == nil {
						argv[i] = reflect.ValueOf(o)
					} else {
						argv[i] = reflect.Zero(funArgType)
					}
				case C.af_jarray:
					l := int(uintptr(arrArgs[j]))
					s := (*C.char)(arrArgs[j+1])
					j += 2
					b := toBytes(s, l)
					var a []interface{}
					if json.Unmarshal(b, &a) == nil {
						argv[i] = reflect.ValueOf(a)
					} else {
						argv[i] = reflect.Zero(funArgType)
					}
				}

				if !argv[i].Type().ConvertibleTo(funArgType) {
					*res_type = C.rt_none
					*out_res = unsafe.Pointer(uintptr(0))
					return
				}
			}
			r = fun.Call(argv)
		}
	}

	if r == nil || len(r) > 1 {
		*res_type = C.rt_none
		*out_res = unsafe.Pointer(uintptr(0))
		return
	}

	res := r[0].Interface()
	switch res.(type) {
	case bool:
		*res_type = C.rt_bool
		if res.(bool) {
			*out_res = unsafe.Pointer(uintptr(1))
		} else {
			*out_res = unsafe.Pointer(uintptr(0))
		}
	case int:
		*res_type = C.rt_int
		*out_res = unsafe.Pointer(uintptr(res.(int)))
	case float64:
		*res_type = C.rt_double
		f := res.(float64)
		p := (*uint64)(unsafe.Pointer(&f))
		*out_res = unsafe.Pointer(uintptr(*p))
	case string:
		setBuffer(res, out_res, res_type, res_len)
	case []byte:
		setBuffer(res, out_res, res_type, res_len)
	case map[string]interface{}:
		resToJson(res, out_res, res_type, res_len)
	case []interface{}:
		resToJson(res, out_res, res_type, res_len)
	default:
		*res_type = C.rt_none
		*out_res = unsafe.Pointer(uintptr(0))
	}
}
