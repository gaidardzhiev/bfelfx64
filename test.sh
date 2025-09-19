#!/bin/sh

G='\033[0;32m'
R='\033[0;31m'
N='\033[0m'

ARCH=$(uname -m)

[ ! "$ARCH" = "x86_64" ] && exit 1

[ ! -f bfelfx64 ] && make

fprint() {
	 printf "[%s] Test: %-20s Result: %b\n" "$(date '+%Y-%m-%d %H:%M:%S')" "$1" "$2"
}

test_torture() {
	./bfelfx64 tests/torture.bf -o torture_test
	chmod +x torture_test
	capture=$(./torture_test)
	filtered=$(printf "%s" "$capture" | tr -cd 'ZaadlLdgaYm!')
	expected="=ZaadlLdgaYm!"
	[ "$filtered" = "$expected" ] && {
		fprint "Torture Test" "${G}PASSED${N}";
		return 0;
	} || {
		fprint "Torture Test" "${R}FAILED${N}";
		return 	12;
	}
}

{ test_torture; return="$?"; } || exit 1

[ "$return" -eq 0 ] 2>/dev/null || printf "%s\n" "$return"
