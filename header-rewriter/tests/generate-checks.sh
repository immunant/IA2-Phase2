#!/bin/sh
# usage: generate-checks.sh <input.h> <path/to/local/includes> <output.h>
#
# adds llvm lit CHECK annotations to header.h based on header.h.head,
# header.h.typedef-checks, and the functions declared in header.h

list_functions() {
	file=$1
	# skips variadic functions for now
	clang -fno-diagnostics-color -Xclang -ast-dump "$1" | \
	fgrep -A99999 "$1" | \
	pcregrep -M -v 'FunctionDecl.+\n.+BuiltinAttr' | \
	fgrep FunctionDecl | \
	grep -aPzo '(.*(<line|'"$1"').*\n)+' | tr '\0' '\a' | sed -e '/\a/,$d' | \
	fgrep -v inline | \
	fgrep -v ... | \
	sed -r -e 's/.*<.*> [^ ]+ ([^ ]+ )*([^ ]+) \x27.*/\2/'
}

base="${test_case}/include/"

header_path=$1 # e.g. /usr/include/libusb-1.0/libusb.h
include_path=$2 # where the .head and .typedef-checks files live
output_path=$3 # the lit-ready header to write
name=$(basename $header_path)
func_checks=$(for function in $(list_functions $header_path); do
	echo "// LINKARGS: --wrap=$function"
done)
truncate -s 0 "$output_path"
cat "$include_path/${name}.head" >> "$output_path"
cat "$include_path/${name}.typedef-checks" >> "$output_path"
echo "$func_checks" >> "$output_path"
cat "$header_path" >> "$output_path"
