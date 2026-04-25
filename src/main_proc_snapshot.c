#include "db_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>


int set_lock(int fd, int type, int offset, int len){
    struct flock lock;
    lock.l_type = type; 
    lock.l_whence = SEEK_SET;
    lock.l_len = len;
    lock.l_start = offset;
    return fcntl(fd, F_SETLKW, &lock);
}

void update(int fd, proc_record *proc){
    proc_record existing_proc;
    int found_pos = -1;
    lseek(fd, sizeof(db_header), SEEK_SET);
    while((read(fd, &existing_proc, sizeof(proc_record))) == sizeof(proc_record)){
        if(existing_proc.pid == proc->pid){ // ca sa nu dam append infinit cautam aceleasi procese cu pid daca au mai fost introduse in snapshot
            found_pos = lseek(fd, 0, SEEK_CUR) - sizeof(proc_record);
            break;
        }
    }
    if(found_pos != -1){
        set_lock(fd, F_WRLCK, found_pos, sizeof(proc_record)); // daca gasim atunci dam overwrite
        lseek(fd, found_pos, SEEK_SET);
        write(fd, proc, sizeof(proc_record));
        set_lock(fd, F_UNLCK, found_pos, sizeof(proc_record));
    }
    else{
        set_lock(fd, F_WRLCK, 0, sizeof(db_header)); // daca nu incrementam active_records
        int pos_final = lseek(fd, 0, SEEK_END); // aflam ultima pozitie
        set_lock(fd, F_WRLCK, pos_final, sizeof(proc_record));//punem lacatele aferente
        db_header header;
        lseek(fd, 0, SEEK_SET);
        read(fd, &header, sizeof(db_header));
        header.record_count++;
        lseek(fd, 0, SEEK_SET);
        write(fd, &header, sizeof(db_header));

        lseek(fd, pos_final, SEEK_SET); // scriem noul proces
        write(fd, proc, sizeof(proc_record));
        set_lock(fd, F_UNLCK, pos_final, sizeof(proc_record));
        set_lock(fd, F_UNLCK, 0, sizeof(db_header));


    }
}

void parcurge_director(int fd){
    DIR *dir = opendir("/proc");
    if(dir == NULL){
        perror("eroare deschidere director /proc");
        return;
    }
    struct dirent *d;
    while((d = readdir(dir)) != NULL){
        if(strcmp(d->d_name, "..") == 0 || strcmp(d->d_name, ".") == 0) continue;
        int este_proces = 1;
        for(int i = 0; d->d_name[i]; i++){ // in directorul proc/ subdirectoarele care au nume ce contin numai cifre reprezinta procesele sistemului
            if(isdigit(d->d_name[i]) == 0){
                este_proces = 0; // deci daca numele contine vreun alt caracter nu e proces
                break;
            }
        }
        if(este_proces == 1){
            proc_record proc;
            char cale[4096];
            sprintf(cale, "/proc/%s/status", d->d_name); // intram in fisierul status al procesului respectiv
            FILE *f_status = fopen(cale, "r"); // ca sa aflam ppid, comm, state si rss
            memset(&proc, 0, sizeof(proc_record)); // alegerea acestui subdirector este argumentata in documentatie
            proc.pid = atoi(d->d_name);
            char linie[256];
            if(f_status != NULL){
                while(fgets(linie, sizeof(linie), f_status)){ // citim linie cu linie din fisier 
                    if(strncmp(linie, "PPid:", 5) == 0){ // facem o cautare usoara dupa primele caractere care reprezinta informatia necesara
                        proc.ppid = atoi(linie + 5); // s-ar putea sa fie spatii intre ppid: si valoarea numerica insa atoi le ignora
                    }
                    else if(strncmp(linie, "Name:", 5) == 0){
                        sscanf(linie + 5, "%s", proc.comm);
                    }
                    else if(strncmp(linie, "State:", 6) == 0){
                        char *p = linie + 6; // aici trebuie sa sarim peste spatiile goale
                        while(*p == ' ' || *p == '\t') p++;
                        proc.state = *p;
                    }
                    else if(strncmp(linie, "VmRSS:", 6) == 0){
                        proc.rss = strtoull(linie + 6, NULL, 10);
                    }
                }
                fclose(f_status);
            }
            char cale_cmd[4096]; // intram in fisierul cmdline ca sa aflam informatia omonima
            sprintf(cale_cmd, "/proc/%s/cmdline", d->d_name);
            int fd_cmd = open(cale_cmd, O_RDONLY);
            if(fd_cmd != -1){
                int bytes = read(fd_cmd, proc.cmdline, sizeof(proc.cmdline) - 1); // trunchiem calea la marimea specificata in header
                if(bytes > 0){
                    proc.cmdline[bytes] = '\0'; // punem null la finalul sirului
                    for(int i = 0; i < bytes; i++){
                        if(proc.cmdline[i] == '\0'){ // restul le transformam in spatii
                            proc.cmdline[i] = ' ';
                        }
                    }
                }
            close(fd_cmd);
            }
            char cale_stat[4096]; // si nu am putut scapa de fisierul stat care contine toate informatiile ce le am obtinut din status insa cautarea lor e putin mai intortocheata
            // trebuie sa gasim de aici valorile de pe pozitiile 14 si 15 care reprezinta utime si stime, ca sa calculam cpu time total
            sprintf(cale_stat, "/proc/%s/stat", d->d_name);
            int fd_stat = open(cale_stat, O_RDONLY);
            if(fd_stat != -1){
                char buffer[4096];
                int arg_counter = 0;
                int bytes_read;
                long long int utime = 0, stime = 0;
                bytes_read = read(fd_stat, &buffer, 4096);
                int i = 0;
                while(i < bytes_read){
                    if(buffer[i] == ' '){ // sarim totusi peste argumentele inutile, adica numaram nr de spatii dintre ele, cu conditia ca numele comenzii sa nu contina si el spatii
                        arg_counter++; // atunci ar esua programul
                    }
                    if(arg_counter == 13){
                        if(buffer[i] >= '0' && buffer[i] <= '9'){
                            utime = utime * 10 + (buffer[i] - '0');
                        }
                    }
                    if(arg_counter == 14){
                        if(buffer[i] >= '0' && buffer[i] <= '9'){
                            stime = stime * 10 + (buffer[i] - '0');
                        }
                    }
                    i++;
                }
                proc.cpu_time = utime + stime; // de precizat ca valorilea astea nu sunt in secunde ci in clock ticks
                close(fd_stat);
            }            
            update(fd, &proc);
        }
    }
    closedir(dir);
}

int main(int argc, char* argv[]){
    char *data_base = "data/proc.db";
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--db") == 0){
            if(i + 1 < argc){
                data_base = argv[i + 1];
            }
        }
    }
    int fd = open(data_base, O_RDWR | O_CREAT, 0644);
    if(fd == -1){
        perror("eroare deschidere baza de date");
        return 1;
    }
    db_header header;
    set_lock(fd, F_WRLCK, 0, sizeof(db_header));
    if((read(fd, &header, sizeof(db_header))) < (int)sizeof(db_header)){// daca nu citim macar sizeof(db_header) initial, inseamna ca baza de date nu e initializata deci
        memset(&header, 0, sizeof(db_header)); // o initializam acum
        strncpy(header.magic, "PRC1", 5);
        header.format_version = 1;
        header.snapshot_state = STATE_OPEN;
        header.active_writers = 1;
        header.record_count = 0;
        header.snapshot_id = time(NULL);
    }
    else{
        if(header.snapshot_state == STATE_SEALED){ //daca e sealed iesim din program
            printf("snapshot sealed\n");
            set_lock(fd, F_UNLCK, 0, sizeof(db_header));
            close(fd);
            return 1;//returnam 1 pentru a fi util in faza de testare
        }
        header.active_writers++; // daca nu e sealed atunci incrementam active_writers
    }
    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(db_header));
    set_lock(fd, F_UNLCK, 0, sizeof(db_header)); 
    sleep(1);

    parcurge_director(fd);

    set_lock(fd, F_WRLCK, 0, sizeof(db_header)); // lock din nou pentru decrementare active_writers
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