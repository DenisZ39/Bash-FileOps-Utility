#include <stdint.h> //pt uint(8/16/32/64)_t=unsigned integer(numere pozitive intregi)
#include <dirent.h>//pt a deschide foldere si a putea vedea ce e in ele
#include <sys/stat.h>//detalii despre fisier (marime, ultima modificare, etc)
#include <stdlib.h>//pt realpath
#include <stdio.h>

struct db_header{
    char magic[6];
    uint8_t format_version;
    uint64_t snapshot_id;
    uint8_t snapshot_state;
    uint32_t active_writers;
    uint64_t record_count;
}__attribute__((packed));//pentru a nu avea spatii goale intre campuri de la unit

#define MAX_PATH 4096
struct file_record{
    char abspath[MAX_PATH];
    uint8_t type;
    uint64_t size;
    uint64_t mtime;
    uint8_t checksum[32];
    uint64_t st_dev;
    uint64_t st_ino;
}__attribute__((packed));

uint32_t xor_checksum(const char *path);
void walk_dir(const char *root_p,FILE *output, uint64_t *count);
