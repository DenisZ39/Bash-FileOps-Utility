//#include "db_format.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <stdint.h>
#include <time.h>

typedef struct {
    char magic[4];
    uint32_t format_version;
    uint64_t snapshot_id;
    uint32_t active_writers;
    uint32_t record_count;
    uint8_t snapshot_state;
} db_header;

typedef struct{
    char cale[4096];
    uint32_t type; // fisier (1), director(2), symlink(3), fifo(4)
    uint64_t size; // st_size
    time_t mtime; // st_mtime
    uint32_t checksum; // xor binar
    uint64_t inode; // st_ino
    uint64_t device; // st_dev
} file_record;

#define STATE_OPEN   0
#define STATE_SEALED 1

void set_lock(int fd, int type, int offset, int len){
    struct flock lock;
    lock.l_type = type; 
    lock.l_whence = SEEK_SET;
    lock.l_len = len;
    lock.l_start = offset;
    return fcntl(fd, F_SETLKW, &lock);
}

void parcurge_recursiv(const char* cale, const char* data_base){
    DIR *dir = opendir(cale);
    if(dir == NULL){
        perror("eroare deschidere director");
        return ;
    }
    struct dirent *d;
    struct flock lock;
    struct stat st;
    char cale_noua[4096];
    while((d = readdir(dir)) != NULL){
        if(strcmp(d->d_name, "..") == 0 || strcmp(d->d_name, ".") == 0) continue;
        sprintf(cale_noua, "%s/%s", cale, d->d_name);
        if(lstat(cale_noua, &st) == -1) continue;
        printf("%s | %ld ", cale_noua, st.st_size);
        if(S_ISDIR(st.st_mode)){
            parcurge_recursiv(cale_noua, data_base);
        }
    }
}

int main(int argc, char* argv[]){
    char *director = NULL;
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--root") == 0){
            if(i + 1 < argc){
                director = argv[i + 1];
            }
        }
    }
    char *data_base =  "data/index.db";
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--db") == 0){
            if(i + 1 < argc){
                data_base = argv[i + 1];
            }
        }
    }
    if(director == NULL) return 1; 
    int fd = open(data_base, O_RDWR | O_CREAT, 0644);
    if(fd == -1){
        perror("eroare deschidere baza de date");
        return 2;
    }
    set_lock(fd, F_WRLCK, 0, sizeof(db_header));
    db_header header;
    if((read(fd, &header, sizeof(header))) < sizeof(header)){
        strncpy(header.magic, "IDX1", 4);
        header.format_version = 1;
        header.snapshot_state = STATE_OPEN;
        header.active_writers = 1;
        header.record_count = 0;
        header.snapshot_id = time(NULL);
    }
    else{
        if(header.snapshot_state == STATE_SEALED){
            printf("snapshot sealed\n");
            set_lock(fd, F_UNLCK, 0, sizeof(db_header));
            close(fd);
            return 0;
        }
        header.active_writers++;
    }
    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(db_header));
    set_lock(fd, F_UNLCK, 0, sizeof(db_header));

    parcurge_recursiv(director, data_base);

    set_lock(fd, F_WRLCK, 0, sizeof(db_header));
    lseek(fd, 0, SEEK_SET);
    read(fd, &header, sizeof(db_header));
    header.active_writers--;
    if(header.active_writers == 0){
        header.snapshot_state = STATE_SEALED;
    }
    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(db_header));
    set_lock(fd, F_UNLCK, 0, sizeof(db_header));

    close(fd);
    return 0;   
}