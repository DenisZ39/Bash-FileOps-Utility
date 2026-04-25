# Documentație Format Binar `index.db`

---

## 1. Introducere
Acest document descrie formatul binar utilizat pentru stocarea indexului de fisiere. Formatul este optimizat pentru **acces concurent** si prezinta un **header fix** si 
**înregistrări accesibile prin offset-uri bine definite**

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

