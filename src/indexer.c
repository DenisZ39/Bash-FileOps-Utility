#include "../include/db_format.h"
#include <stdlib.h>//pt realpath
#include <string.h>
#include <dirent.h>//pt a deschide folderele si a putea vedea ce e in ele
#include <sys/stat.h>//detalii despre fisier (marime, ultima modoficare etc)
#include <time.h>//pt time(NULL)

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

void walk_dir(const char *root_p,FILE *output, uint64_t *count){
    DIR *dir=opendir(root_p);
    if(!dir)return;
    
    struct dirent *entry;
    struct stat st;
    while((entry=readdir(dir))!=NULL){
        if(strcmp(entry->d_name, ".")==0 || strcmp(entry->d_name, "..")==0)//ignorăm directoarele curente și părinte
            continue;
        char full_p[4096];
        snprintf(full_p,sizeof(full_p),"%s/%s",root_p,entry->d_name);//construim calea completă către fișierul sau directorul curent
        if(lstat(full_p, &st)==-1)continue;//dacă nu putem obține informații despre fișier, trecem la următorul
        if(S_ISDIR(st.st_mode)){
            walk_dir(full_p,output,count);
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
            fwrite(&record,sizeof(record),1,output);
            (*count)++;
        }
    }
}

int main(int argc,char *argv[]){
    if(argc==1){
        printf("Utilizare: %s <director>\n",argv[0]);
        return 1;
    }
    FILE *db_file=fopen("data/index.db","wb");
    struct db_header header;
    strncpy(header.magic, "IDXI",sizeof(header.magic));
    header.format_version=1;
    header.snapshot_id=(uint64_t)time(NULL);//timpul curent
    header.snapshot_state=1;//1 pt fopen
    header.active_writers=1;//prima instantă care scrie în baza de date
    header.record_count=0;//numarul de inregistrari din baza de date
    fwrite(&header,sizeof(header),1,db_file);
    uint64_t total_files=0;
    walk_dir(argv[1],db_file,&total_files);//parcurgem directorul specificat și scriem în baza de date, actualizând numărul total de fișiere găsite.
    header.record_count=total_files;//actualizăm numărul de înregistrări din header
    header.snapshot_state=0;//0 pt fclose
    fseek(db_file,0,SEEK_SET);//revenim la inceputul fisierului pentru a rescrie headerul actualizat
    fwrite(&header,sizeof(header),1,db_file);//rescriem headerul actualizat
    fclose(db_file);
}