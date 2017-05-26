#include <stdio.h>
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

int blockfile_fd;

// TODO: replace some read/write to pread/pwrite and add mutex to support multi-thread correctly

void load_fatable(const char *path)
{
    ssize_t nbytes;
    fatable_fd = open(path, O_RDWR);
    if (fatable_fd == -1) {
        if (errno == ENOENT) {
            create_fatable(path);
        } else {
            perror("load_fatable() open");
            exit(1);
        }
    }
    off_t offset = lseek(fatable_fd, 0, SEEK_SET);
    if (offset == -1) {
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
    sync_fatable_metadata();
    if (lseek(fatable_fd, sizeof(metadata), SEEK_SET) == -1) {
        perror("load_fatable() lseek");
        exit(1);
    }
    for (block_size_t i = 0; i < metadata.block_num; i++) {
        blockid_data_t blockdata;
        if (i == 0) {
            blockdata = 0;// root dir, init with one block
        } else if (i + 1 == metadata.block_num) {
            blockdata = i;// end of the chain
        } else {
            blockdata = i + 1;// point to the next block, so that they will be string into a chain
        }
        if (write(fatable_fd, &blockdata, sizeof(blockdata)) == -1) {
            perror("create_fatable() write");
            exit(1);
        }
    }
}

void sync_fatable_metadata(void)
{
    if (pwrite(fatable_fd, &metadata, sizeof(metadata), 0) == -1) {
        perror("sync_fatable_metadata() pwrite");
        exit(1);
    }
}

block_size_t get_next_block_id(block_size_t id)
{
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
    return res;
}

void expand_fatable(void)
{
    block_size_t new_block_num = metadata.block_num * MAGNIFICATION;
    if (lseek(fatable_fd, 0, SEEK_END) == -1) {
        perror("expand_fatable() lseek");
        exit(1);
    }
    for (block_size_t i = metadata.block_num; i < new_block_num; i++) {
        blockid_data_t blockdata;
        if (i + 1 == new_block_num) {
            blockdata = metadata.first_free_block_id;// end of the chain
        } else {
            blockdata = i + 1;// point to the next block, so that they will be string into a chain
        }
        if (write(fatable_fd, &blockdata, sizeof(blockdata)) == -1) {
            perror("expand_fatable() write");
            exit(1);
        }
    }
    metadata.first_free_block_id = metadata.block_num;// make first newly allocate block be the first of the chain
    metadata.free_block_num += new_block_num - metadata.block_num;
    metadata.block_num = new_block_num;
    sync_fatable_metadata();
}

block_size_t acquire_block_chain(block_size_t size)
{
    block_size_t head, tail;
    if (size == 0) {
        printerrf("acquire_block_chain(): size is 0!\n");
        exit(1);
    } else if (size >= metadata.free_block_num) {
        expand_fatable();
    }
    head = tail = metadata.first_free_block_id;
    for (block_size_t i = 1; i < size; i++) {
        tail = get_next_block_id(tail);
    }
    metadata.first_free_block_id = get_next_block_id(tail);
    metadata.free_block_num -= size;
    fatable[tail] = tail;// point to it self, mark it as the tail
    sync_fatable_metadata();
    return head;
}

void release_block_chain(block_size_t head)
{
    block_size_t tail = head, size = 1, next;
    while((next = get_next_block_id(tail)) != tail) {
        tail = next;
        size++;
    }
    fatable[tail] = metadata.first_free_block_id;
    metadata.first_free_block_id = head;
    metadata.free_block_num += size;
    sync_fatable_metadata();
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
