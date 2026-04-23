#include "db_format.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

void compare_files(file_record *old, int nr_old, file_record *new, int nr_new, FILE *f_out){
    for(int i = 0; i < nr_old; i++){
        char *nume_cautat = old[i].cale;
        int found = 0;
        for(int j = 0; j < nr_new; j++){
            if(strcmp(nume_cautat, new[j].cale) == 0){
                found = 1;
                if(old[i].checksum != new[j].checksum || old[i].type != new[j].type || old[i].mtime != new[j].mtime || old[i].size != new[j].size){
                    fprintf(f_out, "Modificat: file record %s\n", nume_cautat);
                }
                break;
            }
        }
        if(found == 0){
            fprintf(f_out, "Sters: file record %s\n", nume_cautat);
        }
    }
    for(int i = 0; i < nr_new; i++){
        char *nume_cautat = new[i].cale;
        int found = 0;
        for(int j = 0; j < nr_old; j++){
            if(strcmp(nume_cautat, old[j].cale) == 0){
                found = 1;
                break;
            }
        }
        if(found == 0){
            fprintf(f_out, "Adaugat: file record %s\n", nume_cautat);
        }
    }
}
void compare_procs(proc_record *old, int nr_old, proc_record *new, int nr_new, FILE *f_out){
    for(int i = 0; i < nr_old; i++){
        int pid_cautat = old[i].pid;
        int found = 0;
        for(int j = 0; j < nr_new; j++){
            if(pid_cautat == new[j].pid){
                found = 1;
                if(abs(old[i].rss - new[j].rss) > 1024 || (new[j].cpu_time - old[i].cpu_time) > 100 || 
                    (old[i].state != new[j].state && (new[j].state == 'T' || new[j].state == 'Z'))){
                        fprintf(f_out, "Modificat: proces record %d\n", pid_cautat);
                }
                break;
            }
        }
        if(found == 0){
            fprintf(f_out, "Sters: proces record %d\n", pid_cautat);
        }
    }
    for(int i = 0; i < nr_new; i++){
        int pid_cautat = new[i].pid;
        int found = 0;
        for(int j = 0; j < nr_old; j++){
            if(pid_cautat == old[j].pid){
                found = 1;
                break;
            }
        }
        if(found == 0){
            fprintf(f_out, "Adaugat: proces record %d\n", pid_cautat);
        }
    }
}

int main(int argc, char* argv[]){
    char *db_old = NULL, *db_new = NULL;
    char *out;
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--old") == 0){
            if(i + 1 < argc){
                db_old = argv[i + 1];
            }
        }
        if(strcmp(argv[i], "--new") == 0){
            if(i + 1 < argc){
                db_new = argv[i + 1];
            }
        }
        if(strcmp(argv[i], "--out") == 0){
            if(i + 1 < argc){
                out = argv[i + 1];
            }
        }
    }
    int fd_old = open(db_old, O_RDONLY);
    int fd_new = open(db_new, O_RDONLY);
    FILE *f_out = fopen(out, "w");
    if(fd_old == -1){
        fprintf(stderr, "eroare deschidere baza de date old");
        if(fd_new != -1){
            close(fd_new);
        }
        return 1;
    }
    if(fd_new == -1){
        fprintf(stderr, "eroare deschidere baze de date new");
        if(fd_old != -1){
            close(fd_old);
        }
        return 1;
    }
    if(f_out == NULL){
        fprintf(stderr, "eroare deschidere fisier reports");
        close(fd_new);
        close(fd_old);
        return 4;
    }
    db_header h1, h2;
    read(fd_old, &h1, sizeof(db_header));
    read(fd_new, &h2, sizeof(db_header));
    if(strcmp(h1.magic, h2.magic) != 0){
        fprintf(stderr, "snapshoturi de tip diferit");
        close(fd_old);
        close(fd_new);
        return 2;
    }
    if(h1.format_version != h2.format_version){
        fprintf(stderr, "snapshoturi de format diferit");
        close(fd_old);
        close(fd_new);
        return 3;
    }
    if(strcmp(h1.magic, "IDX1") == 0){
        file_record *records_old = malloc(h1.record_count * sizeof(file_record));
        file_record *records_new = malloc(h2.record_count * sizeof(file_record));
        read(fd_old, records_old, h1.record_count * sizeof(file_record));
        read(fd_new, records_new, h2.record_count * sizeof(file_record));

        compare_files(records_old, h1.record_count, records_new, h2.record_count, f_out);
        free(records_old);
        free(records_new);
    }
    else if(strcmp(h1.magic, "PRC1") == 0){
        proc_record *records_old = malloc(h1.record_count * sizeof(proc_record));
        proc_record *records_new = malloc(h2.record_count * sizeof(proc_record));
        read(fd_old, records_old, h1.record_count * sizeof(proc_record));
        read(fd_new, records_new, h2.record_count * sizeof(proc_record));

        compare_procs(records_old, h1.record_count, records_new, h2.record_count, f_out);
        free(records_old);
        free(records_new);
    }
    close(fd_old);
    close(fd_new);
    fclose(f_out);
}
