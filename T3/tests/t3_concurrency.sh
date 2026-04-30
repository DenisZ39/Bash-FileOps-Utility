#!/bin/bash
echo "Rulez test 3: concurrency T3"

echo "[1/10] Se curăță datele vechi și se pregătesc folderele"
rm -f data/*.db reports/*.txt
rm -rf tests/dir1 tests/dir2 tests/dir3
mkdir -p data reports tests/dir1 tests/dir2 tests/dir3
sleep 0.5

echo "[2/10] Generăm fișiere de test pentru indexer"
for i in {1..50}; do touch tests/dir1/file_$i.txt; done
for i in {1..50}; do touch tests/dir2/file_$i.txt; done
sleep 0.5

echo "--- ETAPA: FILE INDEXER ---"
echo "[3/10] Lansăm 3 instanțe concurente de fileops_indexer"

records_number=$(find tests/dir1 tests/dir2 src/ -mindepth 1 | wc -l)
expected_size=$((40 + records_number * 4144))


./tools/fileops.sh run -- fileops_indexer --root "tests/dir1/" & ./tools/fileops.sh run -- fileops_indexer --root "tests/dir2/" & ./tools/fileops.sh run -- fileops_indexer --root "src/" &
wait
echo "Instanțele au terminat execuția."
sleep 1

actual_size=$(stat -c"%s" data/index.db)
echo "Verificăm integritatea binară a data/index.db:"
echo "Dimensiune așteptată: $expected_size | Dimensiune reală: $actual_size"

if [ "$expected_size" -eq "$actual_size" ]; then
    echo "Succes: Snapshot creat corect, fără corupere de date."
else
    echo "EROARE: Discrepanță de mărime detectată! Fișierul este corupt."
    exit 2
fi
sleep 1


echo "[4/10] Aplicăm modificări și creăm al doilea snapshot (v2)..."
echo "modificat" > tests/dir1/file_1.txt
rm tests/dir2/file_1.txt
touch tests/dir2/file_100.txt

./tools/fileops.sh run -- ./fileops_indexer --root "tests/dir1/" --db data/index_v2.db & 
./tools/fileops.sh run -- ./fileops_indexer --root "src/" --db data/index_v2.db & 
./tools/fileops.sh run -- ./fileops_indexer --root "tests/dir2/" --db data/index_v2.db &
wait
sleep 0.5

snapshoturi_totale=$(ls data/*.db | wc -l)
echo "Verificăm numărul de fișiere .db create în folderul data: $snapshoturi_totale"
if [ "$snapshoturi_totale" -ge 2 ]; then
    echo "Corect: Instanțele concurente au contribuit la aceleași fișiere (nu s-au creat dubluri)."
else
    echo "Eroare: Număr incorect de baze de date create ($snapshoturi_totale)."
    exit 6
fi
sleep 1

echo "[5/10] Testăm protecția la scriere (starea SEALED)..."
echo "Încercăm să adăugăm date în index_v2.db după ce a fost marcat ca sigilat..."

if ! ./tools/fileops.sh run -- ./fileops_indexer --root "tests/dir3/" --db data/index_v2.db; then
    echo "OK: Acțiunea a eșuat conform așteptărilor (Snapshot-ul este sigilat)."
else
    echo "EROARE: Indexerul a permis scrierea într-un snapshot care ar fi trebuit să fie SEALED!"
    exit 3
fi
sleep 1


echo "[6/10] Generăm raportul DIFF pentru fișiere..."
./tools/fileops.sh run -- ./db_diff --old data/index.db --new data/index_v2.db --out reports/T3_filediff.txt

if [ -s reports/T3_filediff.txt ]; then
    echo "Succes: Raportul T3_filediff.txt a fost generat și are conținut."
else
    echo "EROARE: Raportul de diferențe este gol sau nu a fost creat."
    exit 1
fi
echo "   TRECEM LA TESTAREA PROCESELOR (proc_snapshot)   "
echo "[7/10] Lansăm 3 instanțe concurente de proc_snapshot..."
./tools/fileops.sh run -- ./proc_snapshot &
./tools/fileops.sh run -- ./proc_snapshot &
./tools/fileops.sh run -- ./proc_snapshot &
wait
echo "Instanțele de procese au terminat execuția."
sleep 1


expected_size=$(./tools/fileops.sh run -- db_inspector data/proc.db)
actual_size=$(stat -c"%s" data/proc.db)

echo "Integritate Proc DB: Expected: $expected_size | Actual: $actual_size"
if [ "$actual_size" -eq "$expected_size" ]; then
    echo "Succes: Snapshot-ul de procese este integru."
else
    echo "EROARE: Discrepanță de mărime la baza de date de procese!"
    exit 5
fi
sleep 1

echo "[8/10] Creăm al doilea snapshot de procese (v2)..."
./tools/fileops.sh run -- ./proc_snapshot --db data/proc_v2.db &
./tools/fileops.sh run -- ./proc_snapshot --db data/proc_v2.db &
./tools/fileops.sh run -- ./proc_snapshot --db data/proc_v2.db &
wait
sleep 0.5

echo "Verificam cate snapshoturi s-au creat pentru a vedea daca instantele au folosit acelasi snapshot"
snapshoturi_totale=$(ls data | tr " " "\n" | wc -l)
if [ $snapshoturi_totale -eq 4 ]; then
    echo "Nr total de snapshoturi create = $snapshoturi_totale
            CORECT!"
else
    echo "Nr de snapshoturi create incorect ($snapshoturi_totale)" 
    exit 7
fi

echo "Incercam sa scriem din nou in acelasi snapshot ca sa verificam starea sealed"

echo "[9/10] Verificăm starea SEALED pentru procese..."
if ! ./tools/fileops.sh run -- ./proc_snapshot --db data/proc_v2.db; then
    echo "OK: Accesul refuzat! Snapshot-ul de procese este sigilat corect."
else
    echo "EROARE: S-a permis scrierea în snapshot-ul de procese sigilat!"
    exit 3
fi
sleep 1  

echo "[10/10] Generăm raportul DIFF final pentru procese..."
./tools/fileops.sh run -- ./db_diff --old data/proc.db --new data/proc_v2.db --out reports/T3_procdiff.txt

if [ -s reports/T3_procdiff.txt ]; then
    echo "Succes: Raportul T3_procdiff.txt a fost generat."
else
    echo "Notă: Raportul de procese este gol (posibil nicio modificare semnificativă detectată)."
fi

echo "TESTE FINALIZATE"
