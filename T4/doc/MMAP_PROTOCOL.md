# Documentație Protocol MMAP si Sincronizare

---

## 1. Introducere
Acest document descrie modul în care procesul manager comunică cu procesele worker. Am ales ca schimbul principal de date să se facă printr un fișier IPC mapat în memorie folosind funcția `mmap` cu flag ul `MAP_SHARED`
Formatul memoriei partajate conține coada de job uri, canalele de rezultate si statisticile globale per worker

## 2. Layout ul memoriei partajate
Fișierul mapat conține o singură structură mare care grupează toate datele necesare, protejate de semafoare

### 2.1 Definirea în C
```c
typedef struct{
    char magic[5];
    unsigned int format_version;
    unsigned int is_finished;
    unsigned int file_record_count;
    unsigned int max_depth;
    sem_t mutex_active_jobs;
    int active_jobs;
    struct{
        job buffer[JOB_QUEUE_SIZE]; // 128
        int head;
        int tail;
        sem_t mutex;
        sem_t empty;
        sem_t full;
    } jobs;
    struct{
        file_record buffer[RESULTS_CAPACITY]; // 256
        int head;
        int tail; 
        sem_t mutex;
        sem_t empty;
        sem_t full;
    } results;
    sem_t mutex_stats;
    worker_stats stats[MAX_WORKERS]; // 32
} ipc_shared_data;
```

## 3. Coada de Job uri si Buffer ul Circular
Pentru a gestiona directoarele si rezultatele am ales sa folosim o structura de tip buffer circular peste un vector de capacitate fixa. Capacitatea este stabilita la 128 pentru job uri si 256 pentru canalul de rezultate

Folosim doua pozitii, `head` pentru citire si `tail` pentru scriere care avanseaza modulo dimensiunea maxima a cozii, de exemplu `(shm->jobs.head + 1) % JOB_QUEUE_SIZE`. Managerul plaseaza directorul root pe pozitia 0 la inceput, dupa care workerii introduc la randul lor job uri in coada cand dau de subdirectoare

## 4. Backpressure si Sincronizare
Pentru a preveni ca mai multe procese sa suprascrie datele in memorie in acelasi timp, am initializat semafoarele in structura `shm` de tip POSIX inter proces argumentul 2 din `sem_init` fiind 1
Accesul propriu zis la zonele din structura se fac prin semafoarele `mutex`

Pentru backpressure am folosit cate un sistem de semafoare `empty` si `full` la fiecare buffer. De exemplu, la rezultate daca workerul constata ca vectorul e plin, `sem_wait(&shm->results.empty)` il va forta sa astepte pana cand managerul elibereaza un loc. La citirea din manager folosim `sem_trywait(&shm->results.full)` ca sa putem iesi din ciclu daca nu e nimic de citit si s au terminat toate sarcinile

## 5. Regula de terminare
Ca workerii sa stie cand nu mai exista directoare de parcurs, am introdus un contor `active_jobs` protejat de `mutex_active_jobs`. Cand un worker gaseste un subdirector ii da increment, si dupa ce termina de parcurs directorul curent ii da decrement
In paralel managerul verifica la fiecare pas daca `active_jobs == 0` si nu mai vine niciun rezultat. Daca se intampla asta managerul seteaza un flag `is_finished = 1` si deblocheaza manual toti workerii aflati in asteptare facand `sem_post(&shm->jobs.full)` de `workers_number` ori. Workerul vede `is_finished` activat, isi inregistreaza statisticile de rusage si face break din bucla infinita