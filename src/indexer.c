#include "../include/db_format.h"
#include <stdio.h>
#include <stdlib.h>//pt realpath
#include <string.h>
#include <dirent.h>//pt a deschide folderele si a putea vedea ce e in ele
#include <sys/stat.h>//detalii despre fisier (marime, ultima modoficare etc)

uint32_t xor_checksum(const char *path){
    FILE *f=fopen(path, "rb");
    if(!f) return 0;
    unsigned char buffer[4096];
    uint32_t checksum=0;
    size_t bytes_read;
    while ((bytes_read=fread(buffer,1,sizeof(buffer),f))>0)
    {
        for(int i=0;i<bytes_read;i++){
            checksum^=buffer[i];
        }
    }
    fclose(f);
    return checksum;
}

void walk_dir(const char *root_p){
    DIR *dir=opendir(root_p);
    if(!dir)return;
    
    struct dirent *entry;
    struct stat st;
    while((entry=readdir(dir))!=NULL){
        if(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0)
            continue;
        char full_p[4096];
        snprintf(full_p,sizeof(full_p),"%s/%s",root_p,entry->d_name);
        if(lstat(full_p, &st)==-1)continue;
        if(S_ISDIR(st.st_mode)){
            walk_dir(full_p);
        }
        else if(S_ISREG(st.st_mode)){
            uint32_t hash=xor_checksum(full_p);
            printf("Fisier gasit: %s [Hash: %u]\n",full_p,hash);
            struct file_record record;
            record.size=(uint64_t)st.st_size;
            record.mtime=(uint64_t)st.st_mtime;
            record.st_ino=(uint64_t)st.st_ino;
            record.st_dev=(uint64_t)st.st_dev;
            realpath(full_p,record.abspath);
            memcpy(record.checksum,&hash,sizeof(hash));
            record.type=1;
        }
    }
}