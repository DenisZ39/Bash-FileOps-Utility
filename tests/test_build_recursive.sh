#!/bin/bash
echo "Rulez test 2: Build recursiv"
mkdir -p tmp/test_src/app tmp/test_src/lib/include
cat << 'EOF' > tmp/test_src/lib/include/util.h
int util_add(int a, int b);
EOF
cat<< 'EOF' > tmp/test_src/lib/util.c
int util_add(int a, int b) {return a+b; }
EOF
cat << 'EOF' > tmp/test_src/app/main_demo.c
#include <stdio.h>
#include "util.h"
int main() {
	printf("%d\n", util_add(2,3));
	return 0;
}
EOF
export CFLAGS="-Itmp/test_src/lib/include -std=c11 -Wall -Wextra"
./tools/fileops.sh build --src tmp/test_src > /dev/null
if [ ! -x "bin/demo" ]; then
	echo "Fail: executabilul bin/demo nu exista sau nu poate fi rulat"
	exit 1
fi
./bin/demo > tmp/demo_out.txt
Rezultat=$(cat tmp/demo_out.txt)
if [ "$Rezultat" == 5 ]; then
	echo "Programul a returnat corect: 5"
	exit 0
else
	echo "Fail: Programul a returnat '$Rezultat' in loc de 5"
	exit 1
fi
