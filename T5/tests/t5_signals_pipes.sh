#!/bin/bash

rm -f data/*.db reports/*.txt data/*.mmap
rm -rf tmp/test


mkdir -p tmp/test
touch tmp/manager.log

director=tmp/test
for i in {1..25}; do
    director=$director/nivel_$i
    mkdir -p $director
    touch $director/fisier_$i.txt
done

./tools/fileops.sh run -- fileops_manager --root "tmp/test" --workers 5 --ipc "data/ipc.mmap" --db "data/inventory.db" --simulate-work-ms 2 --pid-file "tmp/test/pid_file.txt"  > tmp/manager.log 2>&1 & 

echo "Asteptam manager:"
pid_file=""
while [ -z $pid_file ]; do
    if [ -s "tmp/test/pid_file.txt" ]; then
        pid_file=$(cat "tmp/test/pid_file.txt")
    fi
    sleep 0.1
done



pid_file=$(cat "tmp/test/pid_file.txt")
echo "Manager pornit cu PID = $pid_file"

echo "Trimitem semnal SIGUSR1 catre manager"
kill -SIGUSR1 $pid_file

sleep 0.5
if grep -q "STATUS" tmp/manager.log; then
    echo "Linie de status gasita"
else
    echo "Eroare: Linie de status lipseste"
    exit 3
fi


echo "Trimitem semnal SIGTERM catre manager"
kill -SIGTERM $pid_file

while kill -0 $pid_file 2>/dev/null; do
    sleep 0.2
done

if ps -eo stat,comm | grep -q "^Z.*fileops_worker"; then
    echo "Eroare: Au ramas workeri zombie"
    exit 4
else
    echo "Workerii au fost curatati corect"
fi

if [ -s "data/inventory.db" ]; then
    echo "Baza de date creata cu succes"
else
    echo "Eroare creare baza de date"
    exit 2
fi
if ! ./tools/fileops.sh run -- fileops_manager --db "data/inventory.db" --verify; then
    echo "Eroare la DB verify"
    exit 5
fi

dump=$(./tools/fileops.sh run -- fileops_manager --db "data/inventory.db" --dump)
touch tmp/test/dump_file.txt
echo "$dump" > tmp/test/dump_file.txt
dump_file=tmp/test/dump_file.txt

if ! grep -q "complete=0" $dump_file; then
    echo "Eroare: complete != 0"
    exit 6
else
    echo "Complete = 0"
fi
 
cat tmp/manager.log