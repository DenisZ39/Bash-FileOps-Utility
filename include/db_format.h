#pragma once
#include <stdint.h>
#include <time.h>

#define STATE_OPEN   0
#define STATE_SEALED 1

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