# Documentație Format Binar `index.db`

---

## 1. Introducere
Acest document descrie formatul binar utilizat pentru stocarea indexului de fisiere. Formatul este optimizat pentru **acces concurent** si prezinta un **header fix** si 
**înregistrări accesibile prin offset-uri bine definite**.
Prin inregistrari accesibile prin offseturi bine definite ne referim la accesarea pozitiilor din snapshot utilizand o formula intuitiva, sizeof(db_header) + i * sizeof(file_record / proc_record), unde i e pozitia pe care o cautam.

## 2. Structura Header-ului
Fiecare fișier bază de date începe cu un header de **40 de octeti** (incluzand padding-ul de aliniere).

| Offset | Câmp | Tip | Descriere |
| :--- | :--- | :--- | :--- |
| 0 | `magic` | `char[5]` | Trebuie să fie `IDX1` |
| 8 | `version` | `unsigned int` | Momentan versiunea `1` |
| 16 | `snapshot_id` | `unsigned long` | ID-ul sesiunii (Timpul la care se realizeaza) |
| 24 | `active_writers` | `unsigned int` | Instante curente care scriu |
| 28 | `record_count` | `unsigned int` | Nr de records din snapshot |
| 32 |  `snapshot_state` | `unsigned char` | Starea snapshot-ului (OPEN - 0 / SEALED - 1) | 


### 2.1 Definirea în C
```c
typedef struct {
    char magic[5];
    unsigned int format_version;
    unsigned long snapshot_id;
    unsigned int active_writers;
    unsigned int record_count;
    unsigned char snapshot_state;
} db_header;
```

## 3. Structura File/Proc Records-urilor
De asemenea, fiecare fisier snapshot va contine un numar de header.record_count fie de structuri de file_records, fie de structuri de proc_records

### 3.1 File Record
Un asemenea prototip de file_record va avea **4144 de octeti** (incluzand cei folositi pentru padding)
```c
typedef struct{
    char cale[4096];
    unsigned int type; 
    unsigned long size; 
    time_t mtime; 
    unsigned int checksum; 
    unsigned long inode; 
    unsigned long device; 
} file_record
```

| Offset | Câmp | Tip | Descriere |
| :--- | :--- | :--- | :--- |
| 0 | `cale` | `char[4096]` | Calea absoluta a fisierului |
| 4096 | `type` | `unsigned int` | Tipul fisierului (REG - 1, DIR - 2, LNK - 3, FIFO - 4) |
| 4104 | `size` | `unsigned long` | Dimensiunea fisierului |
| 4112 | `mtime` | `unsigned long` | Timpul ultimei modificarii |
| 4120 | `checksum` | `unsigned int` | Valoarea checksumului XOR |
| 4128 | `inode` | `unsigned long` | Numarul inodului |
| 4136 |  `device` | `unsigned long` | Device-ul | 

### 3.1.1 Algoritm Checksum determinist
Am ales sa citim continutul fisierului in grupuri de cate 4 octeti, si sa facem o operatie de xor cu toate aceste grupuri, lucru care garanteaza daca o modificare se intampla de un numar impar de ori in aceeasi pozitie, atunci checksum-ul se va modifica. Totusi, am luat în calcul că nu poate detecta erori pare care apar în exact aceeasi pozitie în blocuri diferite, dar pentru acest proiect nu am vrut sa ne complicam

### 3.2 Proc Record
Un asemenea prototip de proc_record va avea **544 de octeti** (incluzand cei folositi pentru padding)
```c
typedef struct{
    unsigned int pid;
    unsigned int ppid;
    char state;
    char comm[256];
    char cmdline[256];
    unsigned long cpu_time;
    unsigned long rss;
} proc_record;
```

| Offset | Câmp | Tip | Descriere |
| :--- | :--- | :--- | :--- |
| 0 | `pid` | `unsigned int` | ID-ul procesului |
| 4 | `ppid` | `unsigned int` | ID-ul procesului parinte |
| 8 | `state` | `char` | Starea procesului (S - Running, T - Idle, Z - Zombie) |
| 9 | `comm` | `char[256]` | Numele comenzii |
| 265 | `cmdline` | `char[256]` | Numele complet al comenzii |
| 528 |  `cpu_time` | `unsigned long` | Timpul consumat de CPU (in clock ticks) | 
| 536 | `rss` | `unsigned long` | Memoria RAM consumata | 

### 3.2.1 Trunchierea continutului cmdline la 256 de octeti
Am ales limita de 256 de octeti pentru variabila cmdline pentru alinierea in memorie (2^8), mentinerea dimensiunii structurii totale proc_records la o valoare mica, dar si in mare parte pentru ca la un set de comenzi, primele comenzi sunt de obicei cele mai importante, si in 256 de octeti putem capta atat comenzile cat si argumentele lor esentiale, fara sa irosim majoritatea spatiului cu null terminators '\0' si liste lungi de fisiere sau cai de foldere repetitive.

### 3.2.2 Aflarea atributului RSS din fisierul /status al procesului
Am ales sa folosim fisierul /proc/pid/status, intrucat acesta are o structura mai usor de utilizat fata de fisierul stat, fiind structurat pe linii de tipul cheie:valoare, ceea ce permite citirea unei linii cate o data si verificarea primelor caractere daca se potriveste cu ceea ce cautam noi (ppid, name, vmrss, state), dupa doar transformam valoarea ca sa se potriveasca in atributele unui proc_record.

Puteam folosi si fisierul stat, insa logica ar fi mai greu de inteles, trebuind sa stim structura fisierului dinainte si chiar si asa, implementarea pe care o folosim sa aflam cpu time ar putea da gres daca numele procesului contine spatii, ni se da numaratoarea peste cap, si asa macar doar cpu time o sa fie gresit, restul ok.

### 3.2.3 CPU TIME

Cum am zis mai sus, folosim fisierul stat numai pentru a afla utime (user time) si stime (kernel time), pentru ca acestea nu se regasesc in fisierul status, folosim fisierul stat.

Structura stat: (1) pid, (2) (comm), (3) state, ....., (14) utime, (15) stime, ...
Deci cum ne-am gandit sa procedam e sa presupunem ca numele comenzii nu contine spatii, si astfel doar sa numaram spatiile dintre informatiile fisierului, si cand ajungem la 13 sa calculam valoarea utime, si la fel pentru stime cand ajungem la 14
Dupa, cpu time = utime + stime, si va fi exprimat in clock ticks, nu in secunde, pentru ca nu ne am mai complicat sa transformam.

### 3.2.4 Procese care dispar inainte de a citi informatiile din fisierele precizate mai sus
O sa fie scrise ca proc_records, o sa aiba pid valid, insa o sa aiba atributele respective goale.



## 4.1 Strategia de actualizare a snapshot-urilor

Pentru a nu da append la infinit, verificam mereu daca nu gasim deja un file_record sau proc_record in snapshotul respectiv, cu aceeasi cale sau pid ca file_recordul pe care dorim sa il adaugam. Daca gasim, dam overwrite, daca nu scriem la finalul snapshotului.

De mentionat ca desi nu trebuia, noi am adaugat la file_records sa compare mai intai inode-ul si device-ul ca sa stabileasca egalitatea dintre cele doua inregistrari, chiar daca in enunt se precizeaza numai dupa calea absoluta

## 5.1 Conditii minime pentru ca un snapshot sa fie valid

Pentru ca un snapshot sa fie valid, trebuie verificata daca dimensiunea reala pe disc a fisierului este egala cu dimensiunea teoretica a acestuia (sizeof(db_header)) + header.records_count * sizeof(file_record / proc_record)). Daca nu se respecta conditia, inseamna ca s-au suprascris date (greseli la lacate), sau record_count nu a fost incrementat corect

## 6.1 Reguli de comparare folosite de db_diff

In primul rand, programul db_diff verifica daca snapshoturile deschide au acelasi tip, IDX1 SAU PRC1, daca au aceeasi versiune. In caz contrar, nu se poate realiza programul.

Pentru a compara doua snapshoturi de tip IDX1, programul are doua faze de cautare: 
1) cautare din primul snapshot in al doilea, cu presupunerea ca primul e cel old, si al doilea new, pentru a vedea daca s-au modificat inregistrari sau s-au sters inregistrari
2) cautare din al doilea snapshot in primul, pentru a vedea daca sunt inregistrati noi adaugate

Cand gasim doua inregistrari cu aceeasi cale, verificam daca le difera ori:
1) checksum-ul, ceea ce inseamna ca s-a produs o schimbare a continutului
2) type-ul, insemnand o schimbare a tipului de fisier
3) mtime-ul, schimare al ultimului timp de modificare fisier
4) size-ul, difera dimensiunea


Pentru a compara doua snapshoturi de tip PRC1, cautarea se executa la fel, cand gasim doua proc_records cu acelasi pid, verificam daca:
1) diferenta absoluta a memoriei RAM depaseste 1Mb, am ales pragul de 1024kb, pentru a ignora fluctuatiile minuscule ale memoriei, concentrandu-ne pe schimbari semnificate ce reprezinta un consum mai ridicat de resurse
2) diferenta dintre timpul consumat pe procesor depaseste 100 clock ticks, adica aproximativ o secunda, tot pentru a ignora procese care ruleaza timp de cateva milisecunde
3) state-ul procesului s-a modificat din running in zombie sau stopped, adica un proces s-a oprit sau a devenit fantoma
