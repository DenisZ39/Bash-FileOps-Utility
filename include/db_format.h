#pragma once
#include <stdint.h>
#include <time.h>

#define STATE_OPEN   0
#define STATE_SEALED 1

typedef struct {
    char magic[5];
    unsigned int format_version;
    unsigned long snapshot_id;
    unsigned int active_writers;
    unsigned int record_count;
    unsigned char snapshot_state;
} db_header;

typedef struct{
    char cale[4096];
    unsigned int type; // fisier (1), director(2), symlink(3), fifo(4)
    unsigned long size; // st_size
    time_t mtime; // st_mtime
    unsigned int checksum; // xor binar
    unsigned long inode; // st_ino
    unsigned long device; // st_dev
} file_record;

typedef struct{
    unsigned int pid;
    unsigned int ppid;
    char state;
    char comm[256];
    char cmdline[256];
    unsigned long cpu_time;
    unsigned long rss;
} proc_record;
