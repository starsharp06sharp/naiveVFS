#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "block.h"

int fatable_fd;
struct fatable_metadata metadata;
blockid_data_t *fatable;
/*
    priority: mem_lock > file_lock
*/
pthread_rwlock_t fatable_mem_lock;
pthread_mutex_t fatable_file_lock;

int blockfile_fd;

void init_block_module(void)
{
    pthread_rwlock_init(&fatable_mem_lock, NULL);
    pthread_mutex_init(&fatable_file_lock, NULL);
}

void load_fatable(const char *path)
{
    ssize_t nbytes;
    fatable_fd = open(path, O_RDWR);
    if (fatable_fd == -1) {
        if (errno == ENOENT) {
            create_fatable(path);
            return ;
        } else {
            perror("load_fatable() open");
            exit(1);
        }
    }
    if (lseek(fatable_fd, 0, SEEK_SET) == -1) {
        perror("load_fatable() lseek");
        exit(1);
    }
    nbytes = read(fatable_fd, &metadata, sizeof(metadata));
    if (nbytes == -1) {
        perror("load_fatable() read");
        exit(1);
    } else if (nbytes < sizeof(metadata)) {
        printerrf("load_fatable(): fatable file is broken");
        exit(1);
    }
    fatable = malloc(metadata.block_num * sizeof(blockid_data_t));
    if (fatable == NULL) {
        perror("load_fatable() malloc");
        exit(1);
    }
    for (block_size_t i = 0; i < metadata.block_num; i++) {
        nbytes = read(fatable_fd, fatable + i, sizeof(blockid_data_t));
        if (nbytes == -1) {
            perror("load_fatable() read");
            exit(1);
        } else if (nbytes < sizeof(blockid_data_t)) {
            printerrf("load_fatable(): fatable file is broken");
            exit(1);
        }
    }
}

void create_fatable(const char *path)
{
    fatable_fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fatable_fd == -1) {
        perror("create_fatable() open");
        exit(1);
    }
    metadata.block_num = INIT_BLOCK_NUM;
    metadata.first_free_block_id = 1;// 0 is root directory file
    metadata.free_block_num = metadata.block_num - 1;
    fatable[0] = 0;// root dir, init with one block
    for (block_size_t i = 1; i < metadata.block_num; i++) {
        fatable[i] = i + 1;// point to the next block, so that they will be string into a chain
    }
    fatable[metadata.block_num -1] = metadata.block_num - 1;// end of the chain
    sync_fatable();
}

void sync_fatable(void)
{
    pthread_rwlock_rdlock(&fatable_mem_lock);
    pthread_mutex_lock(&fatable_file_lock);

    if (lseek(fatable_fd, 0, SEEK_SET) == -1) {
        perror("sync_fatable() lseek");
        exit(1);
    }
    if (write(fatable_fd, &metadata, sizeof(metadata)) == -1) {
        perror("sync_fatable() write");
        exit(1);
    }
    for (block_size_t i = 0; i < metadata.block_num; i++) {
        if (write(fatable_fd, fatable + i, sizeof(fatable[i])) == -1) {
            perror("sync_fatable() write");
            exit(1);
        }
    }

    pthread_mutex_unlock(&fatable_file_lock);
    pthread_rwlock_unlock(&fatable_mem_lock);
}

block_size_t get_next_block_id(block_size_t id, bool need_lock)
{
    if (need_lock) {
        pthread_rwlock_rdlock(&fatable_mem_lock);
    }

    block_size_t res;
    if (id >= metadata.block_num) {
        printerrf("get_next_block_id(): block_id(%ud) out of range\n", (unsigned int) id);
        exit(1);
    }
    res = fatable[id];
    if (res >= metadata.block_num) {
        printerrf("get_next_block_id(): bad fatable[%ud]=%ud\n", (unsigned int) id, (unsigned int) res);
        exit(1);
    }

    if (need_lock) {
        pthread_rwlock_unlock(&fatable_mem_lock);
    }

    return res;
}

void expand_fatable(void)
{
    pthread_rwlock_wrlock(&fatable_mem_lock);

    block_size_t new_block_num = metadata.block_num * MAGNIFICATION;
    blockid_data_t *new_fatable = malloc(new_block_num * sizeof(blockid_data_t));
    memcpy(new_fatable, fatable, metadata.block_num * sizeof(blockid_data_t));
    free(fatable);
    fatable = new_fatable;
    for (block_size_t i = metadata.block_num; i < new_block_num; i++) {
        fatable[i] = i + 1;// point to the next block, so that they will be string into a chain
    }
    fatable[new_block_num - 1] = metadata.first_free_block_id;// end of the chain
    metadata.first_free_block_id = metadata.block_num;// make first newly allocate block be the first of the chain
    metadata.free_block_num += new_block_num - metadata.block_num;
    metadata.block_num = new_block_num;

    pthread_rwlock_unlock(&fatable_mem_lock);
}

block_size_t acquire_block_chain(block_size_t size)
{
    block_size_t head, tail;

    pthread_rwlock_wrlock(&fatable_mem_lock);

    if (size == 0) {
        printerrf("acquire_block_chain(): size is 0!\n");
        exit(1);
    } else if (size >= metadata.free_block_num) {
        pthread_rwlock_unlock(&fatable_mem_lock);
        expand_fatable();
        pthread_rwlock_wrlock(&fatable_mem_lock);
    }
    head = tail = metadata.first_free_block_id;
    for (block_size_t i = 1; i < size; i++) {
        tail = get_next_block_id(tail, false);
    }
    metadata.first_free_block_id = get_next_block_id(tail, false);
    metadata.free_block_num -= size;
    fatable[tail] = tail;// point to it self, mark it as the tail

    pthread_rwlock_unlock(&fatable_mem_lock);

    return head;
}

void release_block_chain(block_size_t head)
{
    pthread_rwlock_wrlock(&fatable_mem_lock);

    block_size_t tail = head, size = 1, next;
    while((next = get_next_block_id(tail, false)) != tail) {
        tail = next;
        size++;
    }
    fatable[tail] = metadata.first_free_block_id;
    metadata.first_free_block_id = head;
    metadata.free_block_num += size;

    pthread_rwlock_unlock(&fatable_mem_lock);
}

void open_blockfile(const char *path)
{
    blockfile_fd = open(path, O_RDWR);
    if (blockfile_fd == -1) {
        if (errno == ENOENT) {
            create_blockfile(path);
        } else {
            perror("open_blockfile() open");
            exit(1);
        }
    }
}

void create_blockfile(const char *path)
{
    blockfile_fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (blockfile_fd == -1) {
        perror("create_blockfile() open");
        exit(1);
    }
    // TODO: write rootdir file correctly
}

void read_block(block_size_t id, uint8_t *buf)
{
    int nbytes = pread(blockfile_fd, buf, BLOCK_SIZE, (off_t)id * BLOCK_SIZE);
    if (nbytes == -1) {
        perror("read_block() pread");
    } else if (nbytes < BLOCK_SIZE) {
        memset(buf + nbytes, 0, BLOCK_SIZE - nbytes);
    }
}

void write_block(block_size_t id, uint8_t *buf)
{
    if (pwrite(blockfile_fd, buf, BLOCK_SIZE, (off_t)id * BLOCK_SIZE) == -1) {
        perror("write_block() pwrite");
        exit(1);
    }
}
