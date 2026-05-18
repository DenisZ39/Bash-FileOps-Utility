#include "../include/format.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h>

int main(int argc, char* argv[]){
    char *director_root = NULL;
    int workers_number = 0;
    char *ipc_fisier = "data/ipc.mmap";
    char *db_inventar = "data/inventory.db";
    int max_depth = 0;
    int timp_testare = 0; // --simulate-work-ms
    int verify_flag = 0; // --verify
    int dump_flag = 0; // --dump

    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--root") == 0){
            if(i + 1 < argc){
                director_root = argv[i + 1];
            }
        }
        if(strcmp(argv[i], "--workers") == 0){
            if(i + 1 < argc){
                workers_number = atoi(argv[i + 1]);
            }
        }
        if(strcmp(argv[i], "--ipc") == 0){
            if(i + 1 < argc){
                ipc_fisier = argv[i + 1];
            }
        }
        if(strcmp(argv[i], "--db") == 0){
            if(i + 1 < argc){
                db_inventar = argv[i + 1];
            }
        }
        if(strcmp(argv[i], "--max-depth") == 0){
            if(i + 1 < argc){
                max_depth = atoi(argv[i + 1]);
            }
        }
        if(strcmp(argv[i], "--simulate-work-ms") == 0){
            if(i + 1 < argc){
                timp_testare = atoi(argv[i + 1]);
            }
        }
        if(strcmp(argv[i], "--verify") == 0){
            verify_flag = 1;
        }
        if(strcmp(argv[i], "--dump") == 0){
            dump_flag = 1;
        }
    }
    
    if(verify_flag != 0){ // daca suntem in mod verify
        int fd_verify = open(db_inventar, O_RDONLY); // deschidem inventarul
        if(fd_verify == -1){
            fprintf(stderr, "eroare deschidere inventar in mod verify");
            exit(11);
        }
        db_header header; // citim headerul
        if(read(fd_verify, &header, sizeof(db_header)) != sizeof(db_header)){ // daca nu putem citi headerul inseamna ca fisierul e neinitializat
            fprintf(stderr, "inventar neinitializat");
            exit(10);
        }
        lseek(fd_verify, sizeof(db_header) + header.file_record_count * sizeof(file_record), SEEK_SET); // citim staturile workerilor
        unsigned int files_emitted_total = 0; // pentru a compara header.file_record_count cu numarul de fisiere transmise de catre fiecare worker
        unsigned long long bytes_emitted_total = 0; // pentru a compara dimensiunile tuturor fisierelor transmise de catre workeri cu dimensiunile fisierelor din file_records
        for(unsigned int i = 0; i < header.worker_count; i++){
            worker_stats worker;
            if(read(fd_verify, &worker, sizeof(worker_stats)) < (unsigned)sizeof(worker_stats)){
                fprintf(stderr, "inventar incomplet");
                exit(15);
            }
            files_emitted_total += worker.files_emitted;
            bytes_emitted_total += worker.bytes_emitted;
        }
        if(files_emitted_total != header.file_record_count){ // inconsistenta legata de sincronizarea proceselor
            fprintf(stderr, "inconsistenta numar de file records");
            exit(16);
        }
        
        unsigned long long dimensiune_file_records = 0;
        lseek(fd_verify, sizeof(db_header), SEEK_SET);
        for(unsigned int i = 0; i < header.file_record_count; i++){ // citim dimensiunile tuturor file_recordsurilor
            file_record fisier;
            if(read(fd_verify, &fisier, sizeof(file_record)) < (unsigned)sizeof(file_record)){
                fprintf(stderr, "inventar incomplet");
                exit(15);
            }
            dimensiune_file_records += fisier.size;
        }
        if(dimensiune_file_records != bytes_emitted_total){
            fprintf(stderr, "inconsistenta dimensiune invnetar");
            exit(17);
        }
        if(strcmp(header.magic, "INV4") != 0){
            fprintf(stderr, "magic gresit la inventar");
            exit(12);
        }
        if(header.version != 1){
            fprintf(stderr, "versiune inventar gresita");
            exit(13);
        }
       
        if(header.complete != 1){
            fprintf(stderr, "inventar oprit inainte de a fi terminat");
            exit(15);
        }
        printf("Verificare reusita si contine %u inregistrari", header.file_record_count); // totul in regula
        close(fd_verify);
        exit(0);

    }
    if(dump_flag != 0){ // afisam informatiile headerului
        int fd_inventar = open(db_inventar, O_RDONLY);
        if(fd_inventar == -1){
            fprintf(stderr, "eroare deschidere inventar pentru dump");
            exit(18);
        }
        db_header header;
        lseek(fd_inventar, 0, SEEK_SET);
        if((read(fd_inventar, &header, sizeof(db_header))) < (unsigned)sizeof(db_header)){
            fprintf(stderr, "fisier inventar incomplet/neinitializat");
            exit(19);
        }
        printf("magic=%s\n", header.magic);
        printf("version=%d\ncomplete=%d\nfile_record_count=%d\nworker_count=%d\n", header.version, header.complete, header.file_record_count, header.worker_count);
        exit(0);
    }
    // daca ajungem pana aici inseamna ca suntem in mod inventariere
    //deschidem fisierul ipc si initializam shared memory ul
    if(director_root == NULL || opendir(director_root) == NULL){ // verifica existenta directorului root
        fprintf(stderr, "fisier radacina invalid");
        exit(2);
    }

    int fd = open(ipc_fisier, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if(fd == -1){
        fprintf(stderr, "eroare deschidere fisier partajat");
        exit(1);
    }
    if(ftruncate(fd, sizeof(ipc_shared_data)) == -1){
        fprintf(stderr, "eroare truncare fisier partajat");
        exit(2);
    }
    ipc_shared_data *shm = mmap(NULL, sizeof(ipc_shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shm == MAP_FAILED){
        fprintf(stderr, "eroare creare map");
        exit(3);
    }
    close(fd);
    //initializam variabilele din shm 
    strcpy(shm->magic, "IPC1");
    shm->format_version = 1;
    shm->is_finished = 0;
    shm->active_jobs = 0;
    shm->max_depth = max_depth;
    shm->file_record_count = 0;
    sem_init(&shm->mutex_active_jobs, 1, 1); // semafor pentru operatiile realizate asupra active_jobs si file_record_count
    
    //initializam structura joburilor
    shm->jobs.head = 0;
    shm->jobs.tail = 0;
    sem_init(&shm->jobs.mutex, 1, 1); // pentru a asigura ca un singur proces acceseaza datele din jobs
    sem_init(&shm->jobs.empty, 1, JOB_QUEUE_SIZE); // cat timp e loc pentru scriere
    sem_init(&shm->jobs.full, 1, 0); // pentru citire

    //initializam structura results
    shm->results.head = 0; 
    shm->results.tail = 0;
    sem_init(&shm->results.mutex, 1, 1); // pentru a asigura ca un singur proces acceseaza datele din results 
    sem_init(&shm->results.empty, 1, RESULTS_CAPACITY);
    sem_init(&shm->results.full, 1, 0);
    
    sem_init(&shm->mutex_stats, 1, 1); // semafor pentru worker_stats

    sem_wait(&shm->jobs.empty); // pregatim adaugarea directorului root in lista jobs
    sem_wait(&shm->mutex_active_jobs);

    strncpy(shm->jobs.buffer[0].path, director_root, 4096); 
    shm->jobs.buffer[0].depth = 0;
    shm->jobs.tail = 1;
    shm->active_jobs = 1;

    sem_post(&shm->mutex_active_jobs);
    sem_post(&shm->jobs.full);

    for(int i = 0; i < workers_number; i++){ // cream N workeri
        int pid = fork();
        if(pid == -1){
            perror("eroare fork");
            exit(5);
        }
        if(pid == 0){ // daca suntem unul din procesele create
            char id[11]; // pentru argumentele execl transformat valoarea i in sir de caractere
            char ms[11]; // la fel si pentru timp_simulare
            snprintf(id, sizeof(id), "%d", i);
            snprintf(ms, sizeof(ms), "%d", timp_testare);
            execl("bin/fileops_worker", "fileops_worker", "--worker-id", id, "--ipc", ipc_fisier, "--simulate-work-ms", ms, NULL);
            perror("eroare execl");
            exit(6);
        }
        else{ // daca suntem manager
            sem_wait(&shm->mutex_stats); // initializam worker_stats
            shm->stats[i].worker_id = i;
            shm->stats[i].pid = pid;
            shm->stats[i].files_emitted = 0;
            shm->stats[i].jobs_processed = 0;
            shm->stats[i].bytes_emitted = 0;
            sem_post(&shm->mutex_stats);
        }
    }

    int records_capacity = 1000; // cand citim rezultatele, le punem intr o memorie dinamica, si daca atingem capacitatea maxima, o dublam si continuam citirea
    file_record *all_records = malloc(records_capacity * sizeof(file_record));
    int records_count = 0;

    while(1){
        if(sem_trywait(&shm->results.full) == 0){ // daca putem scadea semaforul results.full continuam, inseamna ca avem un file_record de procesat
            // nu am pus doar wait mai sus pentru a putea termina programul 
            sem_wait(&shm->results.mutex);

            file_record *fisier_citit = &shm->results.buffer[shm->results.head];//citim rezultatul din head
            if(records_count >= records_capacity){ // daca ajungem peste capacitate trebuie marit bufferul
                records_capacity *= 2;
                all_records = realloc(all_records, records_capacity * sizeof(file_record));
            }
            memcpy(&all_records[records_count++], fisier_citit, sizeof(file_record));//copiam fisierul citit in buffer

            shm->results.head = (shm->results.head + 1) % RESULTS_CAPACITY; // incrementam head

            sem_post(&shm->results.mutex);
            sem_post(&shm->results.empty); // s-a eliberat loc in results
        }
        else{ // daca nu avem rezultat gata
            if(shm->active_jobs == 0){ // vedem daca nu cumva s-au terminat toate joburile
                shm->is_finished = 1; // atunci marcam flagul is_finished cu 1
                for(int i = 0; i < workers_number; i++){ // si pentru terminarea determinista a workerilor, incrementam flagul jobs.full de N ori
                    sem_post(&shm->jobs.full);
                }
                break;
            }
            usleep(100); // sleep pentru a nu bloca procesorul
        }
    }

    for(int i = 0; i < workers_number; i++){ // asteptam terminarea workerilor si capturam exit codeurile lor
        int status;
        int pid = waitpid(-1, &status, 0); // pid retine pid-ul unui proces fiu, status - statusul terminariii
        int exit_code = -1;
        if(WIFEXITED(status)){ // daca s-a terminat fara probleme
            exit_code = WEXITSTATUS(status); // aflam ultimii 8 biti
        }
        else if(WIFSIGNALED(status)){ 
            exit_code = 128 + WTERMSIG(status); // pentru a diferentia iesirile normale fata de cele intrerupte de sistem, adaugam 128
        }
        for(int j = 0; j < workers_number; j++){
            sem_wait(&shm->mutex_stats);
            if(shm->stats[j].pid == (unsigned int)pid){ // cautam workerul cu pid-ul selectat mai sus
                shm->stats[j].exit_status = exit_code;
                sem_post(&shm->mutex_stats);
                break;
            }
            sem_post(&shm->mutex_stats);
        }
    }
    //pregatim baza de date finala
    //scriem in header
    db_header header;
    strcpy(header.magic, "INV4");
    header.complete = 1;
    header.version = 1;
    header.worker_count = workers_number;
    header.file_record_count = records_count;

    //cream o baza de date temporara pentru ca la final sa i putem da rename (operatie atomica), la db_inventar
    char *data_base_temp = "tmp/data_base_tmp.db";
    int fd_db = open(data_base_temp, O_RDWR | O_CREAT |O_TRUNC, 0644);
    if(fd_db == -1){
        perror("eroare deschidere data_base final");
        exit(10);
    }
    lseek(fd_db, 0, SEEK_SET);
    write(fd_db, &header, sizeof(db_header)); // scriem headerul
    write(fd_db, all_records, records_count * sizeof(file_record)); // scriem file_Records
    write(fd_db, shm->stats, workers_number * sizeof(worker_stats)); //scriem worker_stats

    sem_destroy(&shm->mutex_active_jobs);
    sem_destroy(&shm->jobs.mutex);
    sem_destroy(&shm->jobs.empty);
    sem_destroy(&shm->jobs.full);
    sem_destroy(&shm->results.mutex);
    sem_destroy(&shm->results.empty);
    sem_destroy(&shm->results.full);
    sem_destroy(&shm->mutex_stats);
    munmap(shm, sizeof(ipc_shared_data)); // distrugem semafoare si map

    close(fd_db);
    rename(data_base_temp, db_inventar); // operatie atomica
    

}
