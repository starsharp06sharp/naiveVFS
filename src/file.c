#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include "file.h"

struct file_metadata metadatas[FILENO_TABLE_SIZE];
bool occupied[FILENO_TABLE_SIZE];

// TODO: 加入基本检查（如fileno是否存在等）， 加入锁

block_size_t get_blockno(file_size_t offset)
{
    file_size_t real_offset = offset + FILE_METADATA_OFFSET;
    return real_offset / BLOCK_SIZE;
}

file_size_t get_inblock_offset(file_size_t offset)
{
    file_size_t real_offset = offset + FILE_METADATA_OFFSET;
    return real_offset % BLOCK_SIZE;
}

fileno_t acquire_fileno(void)
{
    for (fileno_t i = 0; i < FILENO_TABLE_SIZE; i++) {
        if (!occupied[i]) {
            occupied[i] = true;
            return i;
        }
    }
    return -1;
}

void release_fileno(fileno_t fileno)
{
    occupied[fileno] = false;
}

fileno_t open_file(block_size_t first_block_id)
{
    uint8_t block_buf[BLOCK_SIZE];
    read_block(first_block_id, block_buf);
    fileno_t fileno = acquire_fileno();
    if (fileno == -1) {
        printerrf("open_file(): not enough fileno\n");
        exit(1);
    }
    memcpy(metadatas + fileno, block_buf, sizeof(metadatas[fileno]));
    if (metadatas[fileno].first_block_id != first_block_id) {
        printerrf("open_file(): memtadata is broken\n");
        exit(1);
    }
    return fileno;
}

void close_file(fileno_t fileno)
{
    sync_file_metadata(fileno);
    release_fileno(fileno);
}

void sync_file_metadata(fileno_t fileno)
{
    uint8_t block_buf[BLOCK_SIZE];
    read_block(metadatas[fileno].first_block_id, block_buf);
    memcpy(block_buf, metadatas + fileno, sizeof(metadatas[fileno]));
    write_block(metadatas[fileno].first_block_id, block_buf);
}

void get_metadata(fileno_t fileno, struct file_metadata *buf)
{
    memcpy(buf, metadatas + fileno, sizeof(*buf));
}

int read_file(fileno_t fileno, uint8_t *buf, file_size_t size, file_size_t offset)
{
    uint8_t block_buf[BLOCK_SIZE];
    struct file_metadata *file_info = metadatas + fileno;
    block_size_t start_blockno, end_blockno;
    file_size_t start_inblock_offset, end_inblock_offset;
    file_size_t end_offset;
    file_info->access_time = time(NULL);
    sync_file_metadata(fileno);
    if (offset >= file_info->file_size) {
        return 0;
    }
    start_blockno = get_blockno(offset);
    start_inblock_offset = get_inblock_offset(offset);
    if (offset + size <= file_info->file_size) {
        end_offset = offset + size;
    } else {
        end_offset = file_info->file_size;
    }
    end_blockno = get_blockno(end_offset);
    end_inblock_offset = get_inblock_offset(end_offset);
    if (start_blockno == end_blockno) {
        block_size_t blockid = get_n_next_block_id(file_info->first_block_id, start_blockno);
        read_block(blockid, block_buf);
        memcpy(buf, block_buf + start_inblock_offset, end_inblock_offset - start_inblock_offset);
    } else {
        block_size_t current_blockid, current_blockno = start_blockno;
        uint8_t *current_buf_loc = buf;
        current_blockid = get_n_next_block_id(file_info->first_block_id, current_blockno);
        //copy the first block
        read_block(current_blockid, block_buf);
        memcpy(current_buf_loc, block_buf + start_inblock_offset, BLOCK_SIZE - start_inblock_offset);

        current_blockid = get_n_next_block_id(current_blockid, 1);
        current_blockno++;
        current_buf_loc += BLOCK_SIZE - start_inblock_offset;
        //copy other entire block
        while (current_blockno < end_blockno) {
            read_block(current_blockid, current_buf_loc);

            current_blockid = get_n_next_block_id(current_blockid, 1);
            current_blockno++;
            current_buf_loc += BLOCK_SIZE;
        }
        //copy the last block
        read_block(current_blockid, block_buf);
        memcpy(current_buf_loc, block_buf, end_inblock_offset);
    }
    return end_offset - offset;
}

int write_file(fileno_t fileno, const uint8_t *buf, file_size_t size, file_size_t offset)
{
    uint8_t block_buf[BLOCK_SIZE];
    struct file_metadata *file_info = metadatas + fileno;
    block_size_t start_blockno, end_blockno;
    file_size_t start_inblock_offset, end_inblock_offset;
    file_size_t end_offset;
    file_info->access_time = file_info->modify_time = time(NULL);
    start_blockno = get_blockno(offset);
    start_inblock_offset = get_inblock_offset(offset);
    end_offset = offset + size;
    end_blockno = get_blockno(end_offset);
    end_inblock_offset = get_inblock_offset(end_offset);
    if (end_offset > file_info->file_size) {
        file_info->file_size = end_offset;
        if (end_blockno >= file_info->block_count) {
            block_size_t new_chain_head = acquire_block_chain(end_blockno + 1 - file_info->block_count);
            merge_block_chain(file_info->first_block_id, new_chain_head);
            file_info->block_count = end_blockno + 1;
        }
    }
    sync_file_metadata(fileno);    
    if (start_blockno == end_blockno) {
        block_size_t blockid = get_n_next_block_id(file_info->first_block_id, start_blockno);
        read_block(blockid, block_buf);
        memcpy(block_buf + start_inblock_offset, buf, end_inblock_offset - start_inblock_offset);
        write_block(blockid, block_buf);
    } else {
        block_size_t current_blockid, current_blockno = start_blockno;
        const uint8_t *current_buf_loc = buf;
        current_blockid = get_n_next_block_id(file_info->first_block_id, current_blockno);
        //write the first block
        read_block(current_blockid, block_buf);
        memcpy(block_buf + start_inblock_offset, current_buf_loc, BLOCK_SIZE - start_inblock_offset);
        write_block(current_blockid, block_buf);

        current_blockid = get_n_next_block_id(current_blockid, 1);
        current_blockno++;
        current_buf_loc += BLOCK_SIZE - start_inblock_offset;
        //write other entire block
        while (current_blockno < end_blockno) {
            write_block(current_blockid, current_buf_loc);

            current_blockid = get_n_next_block_id(current_blockid, 1);
            current_blockno++;
            current_buf_loc += BLOCK_SIZE;
        }
        //write the last block
        read_block(current_blockid, block_buf);
        memcpy(block_buf, current_buf_loc, end_inblock_offset);
        write_block(current_blockid, block_buf);
    }
    return end_offset - offset;
}

void read_dir(fileno_t fileno, struct dir_record *dest)
{
    int filename_len;
    struct file_metadata *file_info = metadatas + fileno;
    if (file_info->mode != MODE_ISDIR) {
        printerrf("read_dir(): given fileno isn't dir\n");
        exit(1);
    }
    uint8_t *raw_buf = malloc(file_info->file_size), *raw_buf_pos = raw_buf;
    read_file(fileno, raw_buf, file_info->file_size, 0);// read the hole file
    memcpy(&dest->file_count, raw_buf_pos, sizeof(file_count_t));
    raw_buf_pos += sizeof(file_count_t);

    dest->list_first_block_id = malloc(dest->file_count * sizeof(block_size_t));
    dest->list_filename = malloc(dest->file_count * sizeof(char *));
    for (file_count_t i = 0; i < dest->file_count; i++) {
        if (raw_buf_pos >= raw_buf + file_info->file_size) {
            printerrf("read_dir(): bad file_size\n");
            exit(1);
        }
        memcpy(&(dest->list_first_block_id[i]), raw_buf_pos, sizeof(block_size_t));
        raw_buf_pos += sizeof(block_size_t);

        filename_len = strlen((const char *)raw_buf_pos) + 1;
        dest->list_filename[i] = malloc(filename_len * sizeof(char));
        memcpy(dest->list_filename[i], raw_buf_pos, filename_len);
        raw_buf_pos += filename_len;
    }
}

void create_file(fileno_t dir_fileno, const char *filename, bool is_dir)
{
    int filename_len = strlen(filename) + 1;
    uint8_t buf[sizeof(block_size_t) + filename_len];
    fileno_t fileno = acquire_fileno();
    struct file_metadata *fileinfo = metadatas + fileno;
    fileinfo->first_block_id = acquire_block_chain(1);
    fileinfo->block_count = 1;
    fileinfo->file_size = 0;
    fileinfo->mode = is_dir ? MODE_ISDIR : MODE_ISREG;
    fileinfo->create_time = fileinfo->modify_time = fileinfo->access_time = time(NULL);
    memcpy(buf, &fileinfo->first_block_id, sizeof(block_size_t));
    memcpy(buf + sizeof(block_size_t), filename, filename_len);
    write_file(dir_fileno, buf, sizeof(buf), metadatas[dir_fileno].file_size);// write info in dir
    if (is_dir) {
        init_empty_dir(fileno, fileinfo->first_block_id, metadatas[dir_fileno].first_block_id);
    }
    close_file(fileno);// sync metadata
}

void init_empty_dir(fileno_t fileno, block_size_t block_id, block_size_t father_block_id)
{
    uint8_t buf[EMPTY_DIR_SIZE];
    int buf_off = 0;
    file_count_t c = 2;
    memcpy(buf, &c, sizeof(file_count_t));
    buf_off += sizeof(file_count_t);
    memcpy(buf + buf_off, &block_id, sizeof(block_size_t));
    buf_off += sizeof(block_size_t);
    memcpy(buf + buf_off, ".", sizeof("."));
    buf_off += sizeof(".");
    memcpy(buf + buf_off, &father_block_id, sizeof(block_size_t));
    buf_off += sizeof(block_size_t);
    memcpy(buf + buf_off, "..", sizeof(".."));
    write_file(fileno, buf, EMPTY_DIR_SIZE, 0);
}
