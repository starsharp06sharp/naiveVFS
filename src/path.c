#include "path.h"

int find_next_slash(const char *str, int str_len, int index)
{
    index++;
    while(index < str_len) {
        if (str[index] == '/') return index;
        index++;
    }
    return -1;
}

int read_dir_recursively(const char *_path, struct dir_record *dir)
{
    int path_len = strlen(_path);
    char path[path_len + 1];
    strcpy(path, _path);
    int start = 0, end;
    int fi;
    block_size_t block_id;
    read_dir(0, dir);// read rootdir
    while((end = find_next_slash(path, path_len, start)) != -1) {
        path[end] = '\0';
        fi = find_name_in_dir_record(path + start + 1, dir);
        if (fi == FILE_COUNT_MAX) {
            return -1;
        }
        block_id = dir->list_first_block_id[fi];
        destruct_dir_record(dir);
        fileno_t fh = open_file(block_id);
        read_dir(fh, dir);
        path[end] = '/';
        start = end;
    }
    return start;
}
