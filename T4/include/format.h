
#include <limits.h>
#include <semaphore.h>

#define MAX_WORKERS 32
#define JOB_QUEUE_SIZE 128
#define RESULTS_CAPACITY 256

typedef struct{
    char path[PATH_MAX];
    unsigned long size;
    unsigned long mtime;
    unsigned int mode;
    unsigned int uid;
    unsigned int gid;
    unsigned char sha256[32];
} file_record;

typedef struct{
    int worker_id;
    unsigned int pid;
    int exit_status;
    unsigned int jobs_processed;
    unsigned int files_emitted;
    unsigned long bytes_emitted;
    long wall_time_ms;
    long user_cpu_us;
    long sys_cpu_us;
} worker_stats;

typedef struct{
    char path[PATH_MAX];
    int depth;
} job;

typedef struct{
    char magic[5];
    unsigned int format_version;
    unsigned int is_finished;
    int active_jobs;
    struct{
        job buffer[JOB_QUEUE_SIZE];
        int head;
        int tail;
        sem_t mutex;
        sem_t empty;
        sem_t full;
    } jobs;
    struct{
        file_record buffer[RESULTS_CAPACITY];
        int head;
        int tail; 
        sem_t mutex;
        sem_t empty;
        sem_t full;
    } results;

    worker_stats stats[MAX_WORKERS];
} ipc_shared_data;