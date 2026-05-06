# Documentație Format Binar `inventory.db`

---

## 1. Introducere
Acest document descrie formatul binar final salvat de manager pe disc. Fisierul este scris atomic. Mai exact, noi deschidem si scriem intregul continut intr un fisier temporar `tmp/data_base_tmp.db`, iar la final ii dam pur si simplu un apel `rename` ca sa inlocuiasca baza de date ceruta

## 2. Structura Header ului
DB ul incepe cu un header definit de structura `db_header`

| Câmp | Tip | Descriere |
| :--- | :--- | :--- |
| `magic` | `char[5]` | Trebuie să fie `INV4` |
| `version` | `unsigned int` | Versiunea, stabilita la `1` |
| `complete` | `unsigned int` | Setat la `1` cand s a finalizat scrierea cu succes |
| `file_record_count` | `unsigned int` | Numarul total de fisiere regulare din DB |
| `worker_count` | `unsigned int` | Cati workeri s au ocupat de inventar |

## 3. Structura File Records
Imediat sub header se vor gasi un numar de `file_record_count` structuri care reprezinta fisierele inventariate. Pentru citirea si incarcarea lor in memorie in timpul executiei ne am folosit de un vector alocat cu `malloc` caruia ii dam `realloc` sa ii dublam capacitatea de fiecare data cand se umple

### 3.1 Definirea în C
```c
typedef struct{
    char path[PATH_MAX]; // 4096 octeti
    unsigned long size;
    unsigned long mtime;
    unsigned int mode;
    unsigned int uid;
    unsigned int gid;
    unsigned char sha256[32];
} file_record;
```

| Câmp | Tip | Descriere |
| :--- | :--- | :--- |
| `path` | `char[4096]` | Calea absoluta sau relativa a fisierului descoperit |
| `size` | `unsigned long` | Dimensiunea in bytes a fisierului extrasa din `st_size` |
| `mtime` | `unsigned long` | Timpul de modificare extras cu `st_mtime` |
| `mode` | `unsigned int` | Permisiunile luate cu `st_mode` |
| `uid` | `unsigned int` | ID ul detinatorului din `st_uid` |
| `gid` | `unsigned int` | ID ul de grup din `st_gid` |
| `sha256` | `unsigned char[32]` | Reprezentarea pe biti a hash ului sha256sum |

### 3.1.1 Algoritm pentru extragerea si transformarea SHA256
In loc sa includem dependente externe mari, am ales sa ne folosim de aplicatia din Linux. Cu ajutorul `popen` deschidem un terminal, ii dam comanda `"sha256sum <cale_fisier>"` si citim output ul direct ca string. Dupa ce am luat rezultatul hexazecimal in `rezultat_text`, luam fiecare set de doua caractere, transformam literele in cifre in baza 16 (folosind un offset de `'a'` + 10) si formam efectiv byte ul curent pe care il punem pe pozitia corecta din `sha256`

## 4. Structura Worker Stats
La coada bazei de date se vor scrie un numar de `worker_count` statistici

### 4.1 Definirea în C
```c
typedef struct{
    int worker_id;
    unsigned int pid;
    int exit_status;
    unsigned int jobs_processed;
    unsigned int files_emitted;
    unsigned long long bytes_emitted;
    long wall_time_ms;
    long user_cpu_us;
    long sys_cpu_us;
} worker_stats;
```

Timpul de CPU in user si kernel mode se calculeaza de fiecare worker folosind structura interna `rusage` din Linux si functia `getrusage(RUSAGE_SELF, &usage)` chiar pe ultimul rand inainte sa iasa. Salvam secundele calculate direct in `tv_sec`.
Iar statusul de exit il procesam in manager folosind `waitpid` cu macrourile `WIFEXITED` si `WEXITSTATUS`, dar in cazul opririi prin semnal adaugam 128 la `WTERMSIG` ca sa evitam conflictele

## 5. Reguli de validare in Verify Mode
Cand lansam programul cu argumentul `--verify` verificam `magic == "INV4"`, `version == 1` si `complete == 1`

Dar pe langa asta, ne am gandit sa prevenim coruperile cauzate de o desincronizare la numaratoare din cauza semafoarelor, asa ca am adaugat o verificare adecvata:
1. Adunam intr o variabila toate `files_emitted` de pe fiecare `worker_stats` si verificam sa fie egal cu 
`header.file_record_count`
2. Adunam toate valorile `size` din array ul mare de `file_record` si comparam daca `dimensiune_file_records` sunt egale cu toate valorile adunate din sumele `bytes_emitted_total` inregistrate de catre workeri
Daca vreuna din aceste sume esueaza, managerul va returna 16 sau 17 cu un mesaj in stderr specificand sursa de corupere (inconsistenta numar records sau dimensiune)