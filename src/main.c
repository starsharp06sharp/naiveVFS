#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <locale.h>
#include "base.h"
#include "block.h"
#include "file.h"
#include "path.h"

static void *naive_init(struct fuse_conn_info *conn)
{
    init_block_module();
    init_file_module();
    return NULL;
}

static void naive_destroy(void * op)
{
    sync_all_metadatas();
    sync_fatable();
}

static int naive_statfs(const char *path, struct statvfs *stfs)
{
    // temporary solution
    stfs->f_bsize = BLOCK_SIZE;
    stfs->f_frsize = BLOCK_SIZE;
    stfs->f_blocks = BLOCK_COUNT_MAX;
    stfs->f_bfree = stfs->f_bavail = BLOCK_COUNT_MAX - get_used_block_num();
    stfs->f_files = stfs->f_ffree = FILE_COUNT_MAX / 2;
    stfs->f_namemax = MAX_FILENAME_LEN;
    return 0;
}

static int naive_readdir(const char *_path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info)
{
    struct dir_record dir;
    int pathlen = strlen(_path);
    char path[pathlen + 2];
    strcpy(path, _path);
    if (path[pathlen - 1] != '/') {
        path[pathlen] = '/';
        path[++pathlen] = '\0';
    }
    if (read_dir_recursively(path, &dir) == -1) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    for (file_count_t i = 0; i < dir.file_count; i++) {
        if(filler(buf, dir.list_filename[i], NULL, 0)) {
            break;
        }
    }
    return 0;
}

static int naive_mkdir(const char *_path, mode_t mode)
{
    struct dir_record dir;
    int pathlen = strlen(_path);
    char path[pathlen];
    strcpy(path, _path);
    if (path[pathlen - 1] == '/') {
        if (pathlen <= 1) {
            //root dir
            return -EEXIST;
        } else {
            path[pathlen - 1] = '\0';
            pathlen--;
        }
    }
    int last_slash_i = read_dir_recursively(path, &dir);
    const char *filename = path + last_slash_i + 1;
    if (last_slash_i == -1) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    for (file_count_t i = 0; i < dir.file_count; i++) {
        if (strcmp(filename, dir.list_filename[i]) == 0) {
            destruct_dir_record(&dir);
            return -EEXIST;
        }
    }
    fileno_t fn;
    fn = create_file(dir.dir_fileno, filename, true);
    init_empty_dir(fn, dir.dir_fileno);
    destruct_dir_record(&dir);
    return 0;
}

static int naive_rmdir(const char *_path)
{
    int pathlen = strlen(_path);
    char path[pathlen + 1];
    strcpy(path, _path);
    if (path[pathlen - 1] == '/') {
        path[--pathlen] = '\0';
    }
    struct dir_record dir;
    int last_slash_i = read_dir_recursively(path, &dir);
    const char *filename = path + last_slash_i + 1;
    if (last_slash_i == -1) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    file_count_t fi = find_name_in_dir_record(filename, &dir);
    if (fi == FILE_COUNT_MAX) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    fileno_t fn = open_file(dir.list_first_block_id[fi]);
    struct file_metadata fm;
    get_metadata(fn, &fm);
    close_file(fn);
    int res;
    if (fm.mode != MODE_ISDIR) {
        res = -ENOTDIR;
    } else {
        fileno_t fn = open_file(dir.list_first_block_id[fi]);
        struct dir_record subdir;
        read_dir(fn, &subdir);
        file_count_t file_count = subdir.file_count;
        destruct_dir_record(&subdir);
        if (file_count > 2) {
            res = -ENOTEMPTY;
        } else {
            remove_item_in_dir(&dir, fi);
            write_dir(&dir);
            res = 0;
        }
    }
    destruct_dir_record(&dir);
    return res;
}

static int naive_getattr(const char *_path, struct stat *st)
{
    struct dir_record dir;
    struct file_metadata md;
    int pathlen = strlen(_path);
    char path[pathlen + 1];
    strcpy(path, _path);
    if (path[pathlen - 1] == '/') {
        if (pathlen == 1) {
            // root dir
            get_metadata(0, &md);
            st->st_mode = S_IFDIR | 0777;
            st->st_nlink = 2;
            st->st_atim = (struct timespec) {md.access_time, 0};
            st->st_mtim = (struct timespec) {md.modify_time, 0};
            st->st_ctim = (struct timespec) {md.create_time, 0};
            return 0;
        } else {
            path[--pathlen] = '\0';
        }
    }
    int last_slash_i = read_dir_recursively(path, &dir);
    char *filename = path + last_slash_i + 1;
    if (last_slash_i == -1) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    for (file_count_t i = 0; i < dir.file_count; i++) {
        if (strcmp(filename, dir.list_filename[i]) == 0) {
            fileno_t fn = open_file(dir.list_first_block_id[i]);
            get_metadata(fn, &md);
            if (md.mode == MODE_ISDIR) {
                st->st_mode = S_IFDIR | 0777;
                st->st_nlink = 2;
            } else if (md.mode == MODE_ISREG) {
                st->st_mode = S_IFREG | 0777;
                st->st_nlink = 1;
                st->st_size = md.file_size;
            }
            st->st_atim = (struct timespec) {md.access_time, 0};
            st->st_mtim = (struct timespec) {md.modify_time, 0};
            st->st_ctim = (struct timespec) {md.create_time, 0};
            close_file(fn);
            destruct_dir_record(&dir);
            return 0;
        }
    }
    destruct_dir_record(&dir);
    return -ENOENT;
}

static int naive_utimens(const char *_path, const struct timespec ts[2])
{
    struct dir_record dir;
    struct file_metadata md;
    int pathlen = strlen(_path);
    char path[pathlen + 1];
    strcpy(path, _path);
    if (path[pathlen - 1] == '/') {
        if (pathlen == 1) {
            // root dir
            get_metadata(0, &md);
            md.access_time = ts[0].tv_sec;
            md.modify_time = ts[1].tv_sec;
            set_metadata(0, &md);
            return 0;
        } else {
            path[--pathlen] = '\0';
        }
    }
    int last_slash_i = read_dir_recursively(path, &dir);
    char *filename = path + last_slash_i + 1;
    if (last_slash_i == -1) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    for (file_count_t i = 0; i < dir.file_count; i++) {
        if (strcmp(filename, dir.list_filename[i]) == 0) {
            fileno_t fn = open_file(dir.list_first_block_id[i]);
            get_metadata(fn, &md);
            md.access_time = ts[0].tv_sec;
            md.modify_time = ts[1].tv_sec;
            set_metadata(fn, &md);
            close_file(fn);
            destruct_dir_record(&dir);
            return 0;
        }
    }
    destruct_dir_record(&dir);
    return -ENOENT;
}

static int naive_open(const char *path, struct fuse_file_info *info)
{
    struct dir_record dir;
    int last_slash_i = read_dir_recursively(path, &dir);
    const char *filename = path + last_slash_i + 1;
    if (last_slash_i == -1) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    for (file_count_t i = 0; i < dir.file_count; i++) {
        if (strcmp(filename, dir.list_filename[i]) == 0) {
            fileno_t fn = open_file(dir.list_first_block_id[i]);
            info->fh = fn;
            destruct_dir_record(&dir);
            return 0;
        }
    }
    destruct_dir_record(&dir);
    return -ENOENT;
}

static int naive_release(const char *path, struct fuse_file_info *info)
{
    if (!file_opened(info->fh)) {
        return -EBADF;
    }
    close_file(info->fh);
    return 0;
}

static int naive_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
    if (!file_opened(info->fh)) {
        return -EBADF;
    }
    return read_file(info->fh, (uint8_t *) buf, size, offset);
}

static int naive_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
    if (!file_opened(info->fh)) {
        return -EBADF;
    }
    return write_file(info->fh, (uint8_t *) buf, size, offset);
}

static int naive_mknod(const char *path, mode_t mode, dev_t rdev)
{
    if (!S_ISREG(mode)) {
        return -EINVAL;
    }
    struct dir_record dir;
    int last_slash_i = read_dir_recursively(path, &dir);
    const char *filename = path + last_slash_i + 1;
    if (last_slash_i == -1) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    for (file_count_t i = 0; i < dir.file_count; i++) {
        if (strcmp(filename, dir.list_filename[i]) == 0) {
            destruct_dir_record(&dir);
            return -EEXIST;
        }
    }
    create_file(dir.dir_fileno, filename, false);
    destruct_dir_record(&dir);
    return 0;
}

static int naive_unlink(const char *path)
{
    struct dir_record dir;
    int last_slash_i = read_dir_recursively(path, &dir);
    const char *filename = path + last_slash_i + 1;
    if (last_slash_i == -1) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    file_count_t fi = find_name_in_dir_record(filename, &dir);
    if (fi == FILE_COUNT_MAX) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    fileno_t fn = open_file(dir.list_first_block_id[fi]);
    struct file_metadata fm;
    get_metadata(fn, &fm);
    close_file(fn);
    int res;
    if (fm.mode != MODE_ISREG) {
        res = -EPERM;
    } else {
        remove_item_in_dir(&dir, fi);
        write_dir(&dir);
        res = 0;
    }
    destruct_dir_record(&dir);
    return res;
}

static int naive_truncate(const char *path, off_t size)
{
    int pathlen = strlen(path);
    if (size < 0) return -EINVAL;
    if (path[pathlen - 1] == '/') return -EISDIR;
    struct dir_record dir;
    int last_slash_i = read_dir_recursively(path, &dir);
    const char *filename = path + last_slash_i + 1;
    if (last_slash_i == -1) {
        destruct_dir_record(&dir);
        return -ENOENT;
    }
    for (file_count_t i = 0; i < dir.file_count; i++) {
        if (strcmp(filename, dir.list_filename[i]) == 0) {
            fileno_t fn = open_file(dir.list_first_block_id[i]);
            int res;
            if (cut_file(fn, size)) {
                res = 0;
            } else {
                res = -EFBIG;
            }
            close_file(fn);
            destruct_dir_record(&dir);
            return res;
        }
    }
    destruct_dir_record(&dir);
    return -ENOENT;
}

static struct fuse_operations naivefs_oper = {
    .init = naive_init,
    .destroy = naive_destroy,
    .statfs = naive_statfs,
    .readdir = naive_readdir,
    .mkdir = naive_mkdir,
    .rmdir = naive_rmdir,
    .getattr = naive_getattr,
    .utimens = naive_utimens,
    .open = naive_open,
    .release = naive_release,
    .read = naive_read,
    .write = naive_write,
    .mknod = naive_mknod,
    .unlink = naive_unlink,
    .truncate = naive_truncate
};

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "en_US.UTF-8");
    return fuse_main(argc, argv, &naivefs_oper, NULL);
}