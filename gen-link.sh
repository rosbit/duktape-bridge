#!/bin/sh

function createLink() {
	local dest=$1
	local src=$2
	local file=`basename $src`

	if [ ! -e $dest/$file ]; then
		ln -s `pwd`/$src $dest/
	fi
}

createLink "." duk-bridge-go/duk_bridge.c
createLink duktape duk-bridge-go/duktape.c
createLink duktape duk-bridge-go/duk_console.c
createLink duktape duk-bridge-go/duk_module_duktape.c
createLink duktape duk-bridge-go/duk_print_alert.c
