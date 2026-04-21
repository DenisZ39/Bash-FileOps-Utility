#include "../include/db_format.h"
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


int set_lock(int fd, int type, int offset, int len){
    struct flock lock;
    lock.l_type = type; 
    lock.l_whence = SEEK_SET;
    lock.l_len = len;
    lock.l_start = offset;
    return fcntl(fd, F_SETLKW, &lock);
}

void update(int fd, file_record *file){
    file_record existing_record;
    db_header header;
    int found_pos = -1;
    set_lock(fd, F_WRLCK, 0, 0);
    lseek(fd, sizeof(db_header), SEEK_SET);
    while((read(fd, &existing_record, sizeof(file_record))) == sizeof(file_record)){
        if(strcmp(existing_record.cale, file->cale) == 0){
            found_pos = lseek(fd, 0, SEEK_CUR) - sizeof(file_record);
            break;
        }
    }
    if(found_pos != -1){
        lseek(fd, found_pos, SEEK_SET);
        write(fd, file, sizeof(file_record));
    }
    else{
        lseek(fd, 0, SEEK_END);
        write(fd, file, sizeof(file_record));
        lseek(fd, 0, SEEK_SET);
        read(fd, &header, sizeof(header));
        header.record_count++;
        lseek(fd, 0, SEEK_SET);
        write(fd, &header, sizeof(db_header));
    }
    set_lock(fd,F_UNLCK,0,0);
}

uint32_t calculeaza_checksum(const char* cale){
    int fd_fisier=open(cale, O_RDONLY);
    if(fd_fisier==-1)
        return 0;
    uint32_t checksum=0;
    uint32_t buffer=0;
    while(read(fd_fisier,&buffer,sizeof(buffer))>0){
        checksum^=buffer;
        buffer=0;
    }
    close(fd_fisier);
    return checksum;
}

void parcurge_recursiv(const char* cale, int fd){
    DIR *dir = opendir(cale);
    if(dir == NULL){
        perror("eroare deschidere director");
        return ;
    }
    struct dirent *d;
    struct stat st;
    char cale_noua[4096];
    while((d = readdir(dir)) != NULL){
        
        if(strcmp(d->d_name, "..") == 0 || strcmp(d->d_name, ".") == 0) continue;
        sprintf(cale_noua, "%s/%s", cale, d->d_name);
        if(lstat(cale_noua, &st) == -1) continue;
        file_record file;
        memset(&file, 0, sizeof(file_record));
        strncpy(file.cale, cale_noua, sizeof(file.cale) - 1);        
        file.size = st.st_size;
        file.mtime = st.st_mtime;
        file.inode = st.st_ino;
        file.device = st.st_dev;
        if(S_ISREG(st.st_mode)) {
            file.type = 1;
            file.checksum = calculeaza_checksum(cale_noua);
        }
        if(S_ISDIR(st.st_mode)) file.type = 2;
        if(S_ISLNK(st.st_mode)) file.type = 3;
        if(S_ISFIFO(st.st_mode)) file.type = 4;
        update(fd, &file);        

        if(S_ISDIR(st.st_mode)){
            parcurge_recursiv(cale_noua, fd);
        }
    }
    closedir(dir);
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
    if((read(fd, &header, sizeof(header))) < (ssize_t)sizeof(header)){
        memset(&header, 0, sizeof(db_header));
        memcpy(header.magic, "IDX1", 4);
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

    parcurge_recursiv(director, fd);

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