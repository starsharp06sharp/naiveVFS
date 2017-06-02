#ifndef BASE_H
#define BASE_H

#include <stdio.h>
#include <pthread.h>

#define printerrf(...) fprintf(stderr, __VA_ARGS__)

#define FATABLE_FILENAME "fatable.naivedisk"
#define BLOCKFILE_FILENAME "blockfile.naivedisk"

#endif