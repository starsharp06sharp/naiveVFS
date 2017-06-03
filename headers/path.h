#ifndef PATH_H
#define PATH_H

#include <string.h>
#include "file.h"

/*
    find next '/' in path
    return -1 if not found
*/
int find_next_slash(const char *str, int str_len, int index);

/*
    read the dir by path recursively
    set dir as the  inside dir's file list
    return last slash position
    if the path doesn't exist or isn't a dir, return -1
    the path must be absolute path
*/
int read_dir_recursively(const char *_path, struct dir_record *dir);

#endif