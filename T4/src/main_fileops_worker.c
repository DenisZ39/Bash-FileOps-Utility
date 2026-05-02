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
#include <sys/resource.h>
#include <limits.h>

void calculeaza_hash_fisier(const char *cale, unsigned char *hash_rezultat) {
    char comanda[4096];
    
    // comanda ca in terminal sha256sum `data/fisierul_meu.txt`
    snprintf(comanda, sizeof(comanda), "sha256sum \"%s\"", cale);

    // rulăm comanda cu popen
    FILE *terminal_invizibil = popen(comanda, "r");
    if (terminal_invizibil == NULL) {
        return;
    }
    char rezultat_text[100]; 
    // citim rezultatul comenzii care va fi în format text și conține hash ul urmat de numele fișierului
    if (fgets(rezultat_text, sizeof(rezultat_text), terminal_invizibil) != NULL) {
        //transformăm textul citit în 32 bytes
        for (int i = 0; i < 32; i++) {
            sscanf(&rezultat_text[i * 2], "%2hhx", &hash_rezultat[i]);
        }
    }
    pclose(terminal_invizibil);
}

void parcurge_director(char *cale, int depth, int id, int timp, ipc_shared_data *shm){
    DIR *dir = opendir(cale);
    if(dir == NULL){
        fprintf(stderr, "eroare deschidere director");
        return;
    }
    struct dirent *d;
    while((d = readdir(dir)) != NULL){
        if(strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0) continue;
        char cale_noua[4096];
        snprintf(cale_noua, 4096, "%s/%s", cale, d->d_name);
        struct stat st;
        if(lstat(cale_noua, &st) == -1){
            perror("eroare stat");
            return;
        }
        if(S_ISDIR(st.st_mode)){
            sem_wait(&shm->mutex_active_jobs);
            shm->active_jobs++;
            sem_post(&shm->mutex_active_jobs);

            sem_wait(&shm->jobs.empty);
            sem_wait(&shm->jobs.mutex);

            strncpy(shm->jobs.buffer[shm->jobs.tail].path, cale_noua, 4096);
            shm->jobs.buffer[shm->jobs.tail].depth = depth + 1;
            shm->jobs.tail = (shm->jobs.tail + 1) % JOB_QUEUE_SIZE;

            sem_post(&shm->jobs.mutex);
            sem_post(&shm->jobs.full);
        }
        else if(S_ISREG(st.st_mode)){
            if(timp != 0){
                sleep(timp);
            }
            file_record fisier;
            fisier.size = st.st_size;
            fisier.uid = st.st_uid;
            fisier.gid = st.st_gid;
            fisier.mode = st.st_mode;
            fisier.mtime = st.st_mtime;
            calculeaza_hash_fisier(cale_noua, fisier.sha256);
            sem_wait(&shm->mutex_active_jobs);
            shm->file_record_count++;
            sem_post(&shm->mutex_active_jobs);

            sem_wait(&shm->results.empty);
            sem_wait(&shm->results.mutex);
            memcpy(&shm->results.buffer[shm->results.tail], &fisier, sizeof(file_record));
            shm->results.tail = (shm->results.tail + 1) % RESULTS_CAPACITY;

            sem_post(&shm->results.mutex);
            sem_post(&shm->results.full);

            sem_wait(&shm->mutex_stats);
            shm->stats[id].files_emitted++;
            shm->stats[id].bytes_emitted += st.st_size;
            sem_post(&shm->mutex_stats);

        }
    }
}


int main(int argc, char* argv[]){
    int worker_id = -1;
    char *ipc_fisier = "data/ipc.mmap";
    int timp_testare = 0;
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--worker-id") == 0){
            if(i + 1 < argc){
                worker_id = atoi(argv[i + 1]);
            }
        }
        if(strcmp(argv[i], "--ipc") == 0){
            if(i + 1 < argc){
                ipc_fisier = argv[i + 1];
            }
        }
        if(strcmp(argv[i], "--simulate-work-ms") == 0){
            if(i + 1 < argc){
                timp_testare = atoi(argv[i + 1]);
            }
        }
    }
    int fd = open(ipc_fisier, O_RDWR);
    ipc_shared_data *shm = mmap(NULL, sizeof(ipc_shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shm == MAP_FAILED){
        perror("eroare deschidere map workeri");
        exit(1);
    }
    close(fd);
    while(1){
        sem_wait(&shm->jobs.full);
        if(shm->is_finished && shm->active_jobs == 0){
            break;
        }
        sem_wait(&shm->jobs.mutex);
        job j = shm->jobs.buffer[shm->jobs.head];
        shm->jobs.head = (shm->jobs.head + 1) % JOB_QUEUE_SIZE;
        sem_post(&shm->jobs.mutex);
        sem_post(&shm->jobs.empty);
        if((unsigned int)j.depth < shm->max_depth || shm->max_depth == 0){
            parcurge_director(j.path, j.depth, worker_id, timp_testare, shm);
        }
        sem_wait(&shm->mutex_active_jobs);
        shm->active_jobs--;
        sem_post(&shm->mutex_active_jobs);

        sem_wait(&shm->mutex_stats);
        shm->stats[worker_id].jobs_processed++;
        sem_post(&shm->mutex_stats);
    }

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    sem_wait(&shm->mutex_stats);
    shm->stats[worker_id].user_cpu_us = usage.ru_utime.tv_sec;
    shm->stats[worker_id].sys_cpu_us = usage.ru_stime.tv_sec;
    sem_post(&shm->mutex_stats);

    munmap(shm, sizeof(ipc_shared_data));
    exit(0);
}
