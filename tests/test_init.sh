#!/bin/bash
echo "Rulez Test 1: verificare init"
./tools/fileops.sh init > /dev/null
Foldere=("bin" "src" "include" "data" "logs" "reports" "tmp" "tests" "doc" "tools")
Picat=0
for folder in "${Foldere[@]}"; do
	if [ ! -d "$folder" ]; then
		echo "Fail: folderul $folder lipseste!"
		Picat=1
	fi
done
if [ $Picat -eq 0 ]; then
	echo "Toate folderele exista"
	exit 0
else 
	exit 1
fi
