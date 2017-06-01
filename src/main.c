#define FUSE_USE_VERSION 26

#include <fuse.h>
#include "base.h"
#include "block.h"
#include "file.h"

static void *naive_init(struct fuse_conn_info *conn)
{
    
}

static void naive_destroy(void * op)
{

}

static int naive_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info)
{

}

static int naive_open(const char *path, struct fuse_file_info *info)
{

}

static int naive_release(const char *path, struct fuse_file_info *info)
{

}

static int naive_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{

}

static int naive_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{

}

static int naive_mknod(const char *path, mode_t mode, dev_t rdev)
{

}

static struct fuse_operations naivefs_oper = {
    .init = naive_init,
    .destroy = naive_destroy,
    .readdir = naive_readdir,
    .open = naive_open,
    .release = naive_release,
    .read = naive_read,
    .write = naive_write,
    .mknod = naive_mknod
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &naivefs_oper, NULL);
}