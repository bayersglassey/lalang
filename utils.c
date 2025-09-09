#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lalang.h"


// TODO: dynamically allocate this!
#define FILE_BUFFER_SIZE (1024 * 1024)
char file_buffer[FILE_BUFFER_SIZE];

char *read_file(const char *filename, bool binary) {
    FILE *file = fopen(filename, binary? "rb": "r");
    if (!file) {
        fprintf(stderr, "Failed to open file '%s' (binary=%s): ", filename, binary? "true": "false");
        perror(NULL);
        exit(1);
    }
    size_t n = fread(file_buffer, 1, FILE_BUFFER_SIZE - 1, file);
    if (ferror(file)) {
        fprintf(stderr, "Failed to read from file '%s' (binary=%s): ", filename, binary? "true": "false");
        perror(NULL);
        exit(1);
    };
    file_buffer[n] = '\0';
    fclose(file);
    return file_buffer;
}

int get_index(int i, int len, const char *type_name) {
    if (i < 0) {
        if ((i += len) < 0) {
            fprintf(stderr, "Negative index %i into %s of size %i\n", i, type_name, len);
            exit(1);
        }
    } else if (i >= len) {
        fprintf(stderr, "Out-of-bounds index %i into %s of size %i\n", i, type_name, len);
        exit(1);
    }
    return i;
}
