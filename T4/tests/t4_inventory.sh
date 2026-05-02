#!/bin/bash

rm -f data/*.db reports/*.txt
rm -rf tmp/test

mkdir -p tmp/test
director=tmp/test
for i in {1..25}; do
    director=$director/nivel_$i
    mkdir -p $director
    touch $director/fisier_$i.txt
done

./tools/fileops.sh run -- fileops_manager --root "tmp/test" --workers 5 --ipc "data/ipc.mmap" --db "data/inventory.db" --simulate-work-ms 1

if [ -s data/inventory.db ]; then
    echo "Baza de date creata cu succes"
else
    echo "Eroare: baza de date nu a fost creata"
    exit 1
fi

if ./tools/fileops.sh run -- fileops_manager --db data/inventory.db --verify; then
    echo "OK"
else
    echo "Eroare: Verify a esuat"
    exit 2
fi

dump=$(./tools/fileops.sh run -- fileops_manager --db "data/inventory.db" --dump)
if ! grep -q "magic=INV4" <<< "$dump"; then
    echo "Eroare: Format magic invalid"
    exit 3
elif ! grep -q "version=1" <<< "$dump"; then
    echo "Eroare: versiune invalida"
    exit 3
elif ! grep -q "complete=1" <<< "$dump"; then
    echo "Eroare: complete invalid"
    exit 3
elif ! grep -q "file_record_count=25" <<< "$dump"; then
    echo "Eroare: file_record_count invalid"
    exit 3
elif ! grep -q "worker_count=5" <<< "$dump"; then
    echo "Eroare: worker_count invalid"
    exit 3
fi

echo "Test finalizat cu succes"
exit 0

