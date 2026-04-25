#include "db_format.h" // functia build din fileops.sh stie sa includa headerul
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

uint32_t checksum_compute(const char* path){
    int fd = open(path, O_RDONLY);
    if(fd == -1){
        return 0;
    }
    uint32_t suma = 0;
    uint32_t buffer; // citim cate 4 octeti
    int bytes_read;
    while((bytes_read = read(fd, &buffer, sizeof(uint32_t))) > 0){
        if(bytes_read < sizeof(uint32_t)){ // daca ultima citire e incompleta lipim
            uint32_t last_part = 0;
            memcpy(&last_part, &buffer, bytes_read);
            suma ^= last_part;
        }
        else{
            suma ^= buffer;
        }
    }
    close(fd);
    return suma;
}

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
    lseek(fd, sizeof(db_header), SEEK_SET); // sarim mereu peste header cand citim file records
    while((read(fd, &existing_record, sizeof(file_record))) == sizeof(file_record)){
        // chiar daca in descrierea temei se precizeaza ca comparam doua file_recorduri dupa cale, ne-ati mentionat la laborator ca doua fisiere sunt aceleasi daca au acelasi inode si device
        //deci m-am gandit sa adaug si asta in plus, chiar daca nu cere
        if(existing_record.device == file->device && existing_record.inode == file->inode){ // cautam sa vedem daca nu cumva avem deja recordul file in baza de date
            found_pos = lseek(fd, 0, SEEK_CUR) - sizeof(file_record); // ne uitam dupa inode si device
            break;
        }
    }
    if(found_pos != -1){
        set_lock(fd, F_WRLCK, found_pos, sizeof(file_record)); // daca gasim actualizam
        lseek(fd, found_pos, SEEK_SET);
        write(fd, file, sizeof(file_record));
        set_lock(fd, F_UNLCK, found_pos, sizeof(file_record));
    }
    else{
        lseek(fd, sizeof(db_header), SEEK_SET);
        while (read(fd, &existing_record, sizeof(file_record)) == sizeof(file_record)) { 
            if (strcmp(existing_record.cale, file->cale) == 0) { // altfel verificam daca totusi nu exista alt fisier cu acelasi path, dar inode sau device diferite
                found_pos = lseek(fd, 0, SEEK_CUR) - sizeof(file_record); // ceea ce inseamna ca se modifica file recordul respectiv
                break;
            }
        }
        if(found_pos != -1){
            set_lock(fd, F_WRLCK, found_pos, sizeof(file_record));
            lseek(fd, found_pos, SEEK_SET);
            write(fd, file, sizeof(file_record));
            set_lock(fd, F_UNLCK, found_pos, sizeof(file_record));
        }
        else{
            set_lock(fd, F_WRLCK, 0, sizeof(db_header)); // dam lock pe header ca sa incrementam nr records
            int pos_final = lseek(fd, 0, SEEK_END); // aflam ultima pozitie din snapshot pentru append la final
            set_lock(fd, F_WRLCK, pos_final, sizeof(file_record)); // dam lock pe finalul bazei de date ca sa putem scrie file recordul curent
            lseek(fd, 0, SEEK_END);
            write(fd, file, sizeof(file_record));
            lseek(fd, 0, SEEK_SET);
            read(fd, &header, sizeof(header));
            header.record_count++;
            lseek(fd, 0, SEEK_SET);
            write(fd, &header, sizeof(db_header));
            set_lock(fd, F_UNLCK, 0, sizeof(db_header));
            set_lock(fd, F_UNLCK, pos_final, sizeof(file_record));
        }
    }
}


void parcurge_recursiv(const char* cale, int fd){
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
        file_record file;
        memset(&file, 0, sizeof(file_record));
        strncpy(file.cale, cale_noua, sizeof(file.cale) - 1);        
        file.size = st.st_size;
        file.mtime = st.st_mtime;
        file.inode = st.st_ino;
        file.device = st.st_dev;
        file.checksum = 0;
        if(S_ISREG(st.st_mode)) file.type = 1, file.checksum = checksum_compute(cale_noua);
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
    if((read(fd, &header, sizeof(db_header))) < sizeof(db_header)){ // daca initial nu citim sizeof(db_header) biti inseamna ca nu avem un header, adica nu e initializata
        memset(&header, 0, sizeof(db_header)); // baza de date si o initializam acum
        strncpy(header.magic, "IDX1", 5);
        header.format_version = 1;
        header.snapshot_state = STATE_OPEN;
        header.active_writers = 1;
        header.record_count = 0;
        header.snapshot_id = time(NULL);
    }
    else{
        if(header.snapshot_state == STATE_SEALED){ // daca e sealed iesim
            printf("snapshot sealed\n");
            set_lock(fd, F_UNLCK, 0, sizeof(db_header));
            close(fd);
            return 1;
        }
        header.active_writers++; // daca nu incrementam active writers
    }
    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(db_header));
    set_lock(fd, F_UNLCK, 0, sizeof(db_header));
    sleep(1); // adaugat just in case pathul oferit ca argument nu are multe fisiere si se termina citirea lor inainte ca celelalte instante paralele sa inceapa, altfel exista sanse
    // sa se puna snapshot sealed instant


    parcurge_recursiv(director, fd); // parcurgem recursiv

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