#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include "base.h"
#include "block.h"

typedef int32_t fileno_t;
typedef uint32_t file_size_t;
typedef uint32_t file_mode_t;
typedef uint32_t file_count_t;
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
struct dir_record {
    file_count_t file_count;
    block_size_t *list_first_block_id;
    char **list_filename;
};

#define MODE_ISDIR 1
#define MODE_ISREG 0

#define EMPTY_DIR_SIZE \
    (sizeof(file_count_t) + sizeof(block_size_t) + sizeof(".") + sizeof(block_size_t) + sizeof(".."))

#define FILENO_TABLE_SIZE 65536

#define assert_fileno_valid(fileno) \
    if (!file_opened(fileno)) { \
        printerrf("%s(): invalid fileno: %d\n", __FUNCTION__, (int) fileno); \
        exit(1); \
    }

/*
    get a unused fileno
*/
fileno_t acquire_fileno(void);

/*
    release a fileno
*/
void release_fileno(fileno_t fileno);

/*
    check if file is opened
*/
bool file_opened(fileno_t fileno);

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
    sync all opened file's metadata
*/
void sync_all_metadatas(void);

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

/*
    read dir info to dest
    assume dest is valid
*/
void read_dir(fileno_t fileno, struct dir_record *dest);

/*
    create a file in the given dir(given by fileno)
    if it is a dir,create 2 default dir . and .. in it
*/
void create_file(fileno_t dir_fileno, const char *filename, bool is_dir);

/*
    init a empty dir:
    create 2 default dir . and ..
*/
void init_empty_dir(fileno_t fileno, block_size_t block_id, block_size_t father_block_id);

#endif