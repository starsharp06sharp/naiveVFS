#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include "base.h"

typedef uint32_t block_size_t;
typedef uint32_t blockid_data_t;

struct fatable_metadata {
    block_size_t block_num;
    block_size_t free_block_num;
    blockid_data_t first_free_block_id;
};

#define BLOCK_SIZE 4096
#define INIT_BLOCK_NUM 1024
#define MAGNIFICATION 1.5

extern bool need_init_rootdir;

/*
    initial this module
*/
void init_block_module(void);

/*
    open and load the fatable in the given path
    if doesn't exist, create it
*/
void load_fatable(const char *path);

/*
    create fatable and write initial data
*/
void create_fatable(const char *path);

/*
    write current fatable to disk
*/
void sync_fatable(void);

/*
    get next n block's id
    if the block is not enough, return the same value as id
    n should greater than 0
*/
block_size_t get_n_next_block_id(block_size_t id, size_t n);

/*
    acquire a block chian
    if free block is not enough, expand the disksize
*/
block_size_t acquire_block_chain(block_size_t size);

/*
    release a block chian to the free block chain
*/
void release_block_chain(block_size_t head);

/*
    merge the head2 chain to the tail of the head1 chain
*/
void merge_block_chain(block_size_t head1, block_size_t head2);

/*
    open the blockfile in the given path
    if doesn't exist, create it
*/
void open_blockfile(const char *path);

/*
    create blockfile and write initial data
*/
void create_blockfile(const char *path);

/*
    read a block of data
    assume buf is vaild and has at least BLOCK_SIZE bytes of memory
*/
void read_block(block_size_t id, uint8_t *buf);

/*
    write a block of data
    assume buf has at least BLOCK_SIZE bytes of data
*/
void write_block(block_size_t id, const uint8_t *buf);

#endif
