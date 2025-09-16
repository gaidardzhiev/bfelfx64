#!/bin/sh

ARCH=$(uname -m)

[ ! "$ARCH" = "x86_64" ] && exit 1

[ ! -f slug ] && make

fprint() {
	 printf "[%s] Test: %-20s Result: %b\n" "$(date '+%Y-%m-%d %H:%M:%S')" "$1" "$2"
}

test_torture() {
	./bfelfx64 tests/torture.bf -o torture
	chmod +x torture
	capture=$(./torture)
	[ "$capture" = "=ZaadlLdgaYm!#" ] && {
		fprint "Torture Test" "${G}PASSED${N}";
		return 0;
	} || {
		fprint "Torture Test" "${R}FAILED${N}";
		return 	12;
	}
}

test_torture
