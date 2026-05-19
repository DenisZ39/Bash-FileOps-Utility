# Documentatie Control Plane si Gestiune Semnale

## 1 Introducere
Acest document descrie arhitectura de control plane adaugata la T5. Pe langa memoria partajata prin mmap unde tinem datele principale, am facut un canal separat ca sa trimitem mesaje scurte, erori si sa raportam starea proceselor

Comunicarea se face printr un pipe anonim unidirectional de la workeri catre manager

## 2 Arhitectura Control Plane (Pipe Anonim)
Inainte sa porneasca workerii, managerul face un pipe folosind apelul `pipe(control_pipe)`. 

* **Workerii:** Iau capatul de scriere `control_pipe[1]` si isi inchid capatul de citire. Prin capatul de scriere ei baga textele cu mesaje.
* **Managerul:** Inchide capatul de scriere. Pentru ca bucla while(1) a managerului sa nu ramana blocata asteptand mesaje, ne am gandit sa punem capatul de citire `control_pipe[0]` pe modul non-blocant folosind `fcntl` cu flag ul `O_NONBLOCK`.

Cand citeste mesajele, managerul le baga intr un buffer de 4096 octeti si dupa folosim `strtok` cu delimitatorul `\n` ca sa procesam fiecare linie separat.

## 3 Formatul Mesajelor T5MSG
Ca sa ne asiguram ca mesajele nu se amesteca atunci cand sunt trimise, am folosit un singur apel `write` pentru fiecare mesaj, a carui dimensiune este garantat sub `PIPE_BUF`. Orice mesaj incepe cu string ul `T5MSG` si se termin cu `\n`

### 3.1 Formate mesaje implementate in cod

| Tip Mesaj | Format text folosit | Descriere |
| :--- | :--- | :--- |
| `JOB_DONE` | `T5MSG type=JOB_DONE worker_id=%d jobs=%d files=%u bytes=%llu\n` | Trimis dupa ce un worker a terminat de parcurs un director, ca managerul sa vada ce s a produs. |
| `WORKER_EXITING` | `T5MSG type=WORKER_EXITING worker_id=%d reason=%s\n` | Trimis inainte sa iasa workerul. Motivul poate sa fie `normal` sau `shutdown`. |
| `ERROR` | `T5MSG type=ERROR worker_id=%d errno=%d where=opendir\n` | Daca pic apelul opendir pe un director, trimitem mesajul asta sa stim despre ce eroare e vorba.

## 4 Pornirea Workerilor si Argumente CLI
Pornim procesele fiu cu `fork` si dupa le dam un `execl` pe binarul workerului. Interfata din terminal cere sa le pasam ca argumente sub forma de string, inclusiv file descriptorul pentru pipe ca workerii sa stie unde sa scrie.

```c
char id[11], ms[11], fd[11];
snprintf(fd, sizeof(fd), "%d", control_pipe[1]);
snprintf(id, sizeof(id), "%d", i);
snprintf(ms, sizeof(ms), "%d", timp_testare);

execl("bin/fileops_worker", "fileops_worker",
      "--worker-id", id, "--ipc", ipc_fisier,
      "--simulate-work-ms", ms, "--control-fd",
       fd, NULL);
```
Managerul stie sa citeasca urmatoarele argumente noi:

* `--graceful-timeout <sec>` : Cat asteapta managerul ca workerii sa isi termine treaba dupa ce le a dat semnal de oprire. Noi am pus default valoarea de 5 secunde ca sa nu ne complicam.
* `--simulate-work-ms <ms>` : Un sleep pe care il dam ca sa testam mai usor scripturile si semnalele in timp ce procesele ruleaza.
* `--pid-file <path>` : Managerul isi scrie pid ul in fisierul asta ca sa stie scriptul de testare unde sa trimita semnalele cu comanda kill.

## 5 Handlere de Semnale

Pentru ca nu e safe sa faci tot felul de operatii complexe prin handlere (precum printf, malloc sau umblat pe la mmap), noi doar am setat niste flag uri globale de tipul `volatile sig_atomic_t` . Toata logica efectiva este lasata sa se rezolve in bucla principala.

### 5.1 Semnale procesate in Manager
1.  `SIGUSR1` : Setam flag ul `status_requested` . In while ul mare printam o linie cu date la zi despre statusul aplicatiei: `STATUS queued_jobs=... active_jobs=... files=...` .
2.  `SIGINT` / `SIGTERM` : Setam flag ul `shutdown_requested = 1` . In loop managerul vede asta si incepe sa opreasca programul, punand si in mmap variabila `shm->is_finished = 1` .
3.  `SIGCHLD` : Ne anunta ca a picat un proces fiu. Folosim `waitpid` cu `WNOHANG` sa preluam statusul ca sa nu ne ramana procese zombie in sistem.

### 5.2 Semnale procesate in Worker
* Workerul prinde `SIGTERM` de la manager si seteaza `worker_shutdown_requested` .
* Workerul termina ce fisier proceseaza la momentul ala, da break, scrie in pipe ca iese (cu motivul shutdown) si da exit.

## 6 Oprirea Programului si Scrierea Bazei de Date

Cand managerul prinde comanda de oprire, trebuie sa inchida workerii. Algoritmul e urmatorul:

1.  Trimitem `SIGTERM` la toti workerii.
2.  Facem un loop unde asteptam sa vedem daca isi dau exit, verificand statusul cu `WIFEXITED` .
3.  Adunam `128` la semnal cand ii oprim noi ca sa salvam un exit code corect.
4.  **Fallback la forta:** Daca stau mai mult de cele 5 secunde setate la timeout, inseamna ca s au blocat. Managerul le da direct `SIGKILL` si `waitpid` ca sa ii omoare si sa i curete fortat.

### 6.1 Completitudinea Bazei de Date (complete=0)
Aici trebuie sa facem diferenta intre inventariere terminata cap coada si inventariere oprita din mers de catre utilizator.

* **Inventariere normala:** Daca toate job urile s au terminat cu succes si nu mai e nimic activ ( `active_jobs == 0` ), in header punem `complete=1` .
* **Inventariere intrerupta:** Daca programul s a oprit din cauza unui semnal inainte sa se termine toate recuperarile, noi salvam fisierul ca fiind valid structural, dar punem in header variabila `complete=0` ca sa se stie ca inventarul e incomplet.

Scrierea DB-ului o facem la final. Ca sa fie treaba atomica si fara probleme, scriem header-ul si tot restul datelor intr un fisier temporar `tmp/data_base_tmp.db` , iar fix la sfarsit apelam functia `rename` pentru a inlocui fisierul de inventory cerut. Asa trece mereu cu succes testul de `--verify` cu baza de date integrala.