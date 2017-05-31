#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include "base.h"
#include "block.h"

typedef int32_t fileno_t;
typedef uint32_t file_size_t;
typedef uint32_t file_mode_t;
struct file_metadata {
    block_size_t first_block_id;
    block_size_t block_count;
    file_size_t file_size;
    file_mode_t mode;
    time_t create_time;
    time_t access_time;
    time_t modify_time;
};
#define FILE_METADATA_OFFSET (sizeof(struct file_metadata))

#define MODE_ISDIR 1
#define MODE_ISREG 0

#define FILENO_TABLE_SIZE 65536

/*
    get a unused fileno
*/
fileno_t acquire_fileno(void);

/*
    release a fileno
*/
void release_fileno(fileno_t fileno);

/*
    open file from the given block_id
    load the metadata to memory and returns a fileno
*/
fileno_t open_file(block_size_t first_block_id);

/*
    close a file by fileno:
    sync metadata to disk and release this fileno
*/
void close_file(fileno_t fileno);

/*
    sync file's metadata
*/
void sync_file_metadata(fileno_t fileno);

/*
    copy the files metadata to the given buf
*/
void get_metadata(fileno_t fileno, struct file_metadata *buf);

/*
    read a file like pread
*/
int read_file(fileno_t fileno, uint8_t *buf, file_size_t size, file_size_t offset);

/*
    write a file like pwrite
*/
int write_file(fileno_t fileno, const uint8_t *buf, file_size_t size, file_size_t offset);

#endif