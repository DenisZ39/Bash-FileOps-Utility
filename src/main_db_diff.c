#include "db_formats.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int compare_records(const void* a,const void* b){
    const file_record *r1=(const file_record*)a;
    const file_record *r2=(const file_record*)b;
    return strcmp(r1->cale,r2->cale);
}
void compare_db(const char* db1, const char* db2){
    int fdb1=open(db1, O_RDONLY);
    int fdb2=open(db2, O_RDONLY);
    if(fdb1==-1 || fdb2==-1){
        perror("Eroare deschidere baza de date");
        return;
    }
    db_header h1,h2;
    if(read(fdb1,&h1,sizeof(db_header))<sizeof(db_header)){
        perror("Eroare citire header db1");
        close(fdb1);
        close(fdb2);
        return;
    }
    if(read(fdb2,&h2,sizeof(db_header))<sizeof(db_header)){
        perror("Eroare citire header db2");
        close(fdb1);
        close(fdb2);
        return;
    }
    if(memcmp(h1.magic,"IDXI",4)!=0){
        fprintf(stderr,"Fisierul %s nu este o baza de date valida\n",db1);
        close(fdb1);
        close(fdb2);
        return;
    }
    if(memcmp(h2.magic,"IDXI",4)!=0){
        fprintf(stderr,"Fisierul %s nu este o baza de date valida\n",db2);
        close(fdb1);
        close(fdb2);
        return;
    }
    file_record *records1=NULL;
    if(h1.record_count>0){
        records1=malloc(h1.record_count*sizeof(file_record));
        if(records1==NULL){
            perror("Eroare alocare memorie pentru records1");
            free(records1);
            close(fdb1);
            close(fdb2);
            return;
        }
        if(read(fdb1,records1,h1.record_count*sizeof(file_record))<h1.record_count*sizeof(file_record)){
        perror("Eroare citire records1");
        free(records1);
        close(fdb1);
        close(fdb2);
        return;
        }
    }
    file_record *records2=NULL;
    if(h2.record_count>0){
        records2=malloc(h2.record_count*sizeof(file_record));
        if(records2==NULL){
            perror("Eroare alocare memorie pentru records2");
            free(records1);
            free(records2);
            close(fdb1);
            close(fdb2);
            return;
        }
        if(read(fdb2,records2,h2.record_count*sizeof(file_record))<h2.record_count*sizeof(file_record)){
            perror("Eroare citire records2");
            free(records1);
            free(records2);
            close(fdb1);
            close(fdb2);
            return;
        }
    }
    qsort(records1,h1.record_count,sizeof(file_record),compare_records);
    qsort(records2,h2.record_count,sizeof(file_record),compare_records);
    int i=0,j=0;
    while(i<h1.record_count && j<h2.record_count){
        if(strcmp(records1[i].cale,records2[j].cale)==0){
            if(records1[i].size!=records2[j].size || records1[i].mtime!=records2[j].mtime || memcmp(records1[i].checksum,records2[j].checksum,sizeof(records1[i].checksum))!=0){
                printf("Fisier modificat: %s\n",records1[i].cale);
            }
            i++;j++;
        }
        else if(strcmp(records1[i].cale,records2[j].cale)<0){
            printf("Fisier sters: %s\n",records1[i].cale);
            i++;
        }
        else {
            printf("Fisier adaugat: %s\n",records2[j].cale);
            j++;
        }
    }
    while(i<h1.record_count){
        printf("Fisier sters: %s\n",records1[i].cale);
        i++;
    }
    while(j<h2.record_count){
        printf("Fisier adaugat: %s\n",records2[j].cale);
        j++;
    }
    free(records1);
    free(records2);
    close(fdb1);
    close(fdb2);
}
int main(int argc, char* argv[]){
    if(argc!=3){
        printf("Utilizare: %s <db1> <db2>\n", argv[0]);
        return 1;
    }
    compare_db(argv[1], argv[2]);
    return 0;
}
