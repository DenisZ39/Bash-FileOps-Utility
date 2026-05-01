#include "/home/andrei/homework/R11_homework_repo_10/T4/include/format.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char* argv[]){
    char *director_root = NULL;
    int workers_number = 0;
    char *ipc_fisier = "data/ipc.mmap";
    char *db_inventar = "data/inventory.db";
    int max_depth = 0;
    int timp_testare = 0;

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

    strcpy(&shm->magic, "IPC1");
    shm->format_version = 1;
    shm->is_finished = 0;
    shm->active_jobs = 0;
    sem_init(&shm->mutex_active_jobs, 1, 1);
    
    shm->jobs.head = 0;
    shm->jobs.tail = 0;
    sem_init(&shm->jobs.mutex, 1, 1);
    sem_init(&shm->jobs.empty, 1, JOB_QUEUE_SIZE);
    sem_init(&shm->jobs.full, 1, 0);

    shm->results.head = 0;
    shm->results.tail = 0;
    sem_init(&shm->results.mutex, 1, 1);
    sem_init(&shm->results.empty, 1, RESULTS_CAPACITY);
    sem_init(&shm->results.full, 1, 0);

    sem_wait(&shm->jobs.empty);
    sem_wait(&shm->mutex_active_jobs);

    strncpy(shm->jobs.buffer[0].path, director_root, 4096);
    shm->jobs.buffer[0].depth = 0;
    shm->jobs.tail = 1;
    shm->active_jobs = 1;

    sem_post(&shm->mutex_active_jobs);
    sem_post(&shm->jobs.full);

    for(int i = 0; i < workers_number; i++){
        int pid = fork();
        if(pid == -1){
            perror("eroare fork");
            exit(5);
        }
        if(pid == 0){
            char id[10];
            snprintf(id, sizeof(id), "%d", i);
            execl("bin/fileops_worker", "fileops_worker", "--worker-id", id, "--ipc", ipc_fisier, NULL);
            perror("eroare execl");
            exit(6);
        }
    }

    int records_capacity = 1000;
    file_record *all_records = malloc(records_capacity * sizeof(file_record));
    int records_count = 0;

    while(1){
        if(sem_trywait(&shm->results.full) == 0){
            sem_wait(&shm->results.mutex);

            file_record *fisier_citit = &shm->results.buffer[shm->results.head];
            if(records_count >= records_capacity){
                records_capacity *= 2;
                all_records = realloc(all_records, records_capacity * sizeof(file_record));
            }
            memcpy(&all_records[records_count++], fisier_citit, sizeof(file_record));

            shm->results.head = (shm->results.head + 1) % RESULTS_CAPACITY;

            sem_post(&shm->results.mutex);
            sem_post(&shm->results.empty);
        }
        else{
            if(shm->active_jobs == 0){
                break;
            }
            usleep(100);
        }
    }
    for(int i = 0; i < workers_number; i++){
        int status;
        int pid = waitpid(-1, &status, 0);
        int exit_code = -1;
        if(WIFEXITED(status)){
            exit_code = WEXITSTATUS(status);
        }
        else if(WIFSIGNALED(status)){
            exit_code = 128 + WTERMSIG(status);
        }
        for(int j = 0; j < workers_number; j++){
            if(shm->stats[j].pid == pid){
                shm->stats[j].exit_status = exit_code;
                break;
            }
        }
    }

    db_header header;
    strcpy(header.magic, "INV4");
    header.complete = 1;
    header.version = 1;
    header.worker_count = workers_number;
    header.file_record_count = records_count;

    char *data_base_temp = "tmp/data_base_tmp.db";
    int fd = open(data_base_temp, O_RDWR | O_CREAT |O_TRUNC, 0644);
    if(fd == -1){
        perror("eroare deschidere data_base final");
        exit(10);
    }
    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(db_header));
    write(fd, all_records, records_count * sizeof(file_record));
    write(fd, shm->stats, workers_number * sizeof(worker_stats));
    sem_destroy(&shm->mutex_active_jobs);
    sem_destroy(&shm->jobs.mutex);
    sem_destroy(&shm->jobs.empty);
    sem_destroy(&shm->jobs.full);
    sem_destroy(&shm->results.mutex);
    sem_destroy(&shm->results.empty);
    sem_destroy(&shm->results.full);
    munmap(shm, sizeof(ipc_shared_data));

    close(fd);
    rename(data_base_temp, db_inventar);

}
