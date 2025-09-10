#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "lalang.h"


char *read_file(const char *filename, bool required) {
    // Reads a file's contents, either:
    //   * returning a string
    //   * returning NULL if !required and file didn't exist
    //   * exiting if there was an error

    // Open file
    FILE *file = fopen(filename, "r");
    if (!file) {
        if (!required && errno == ENOENT) {
            // No such file
            return NULL;
        } else {
            // Something went horribly wrong
            fprintf(stderr, "Could not open file '%s': ", filename);
            perror(NULL);
            exit(1);
        }
    }

    // Figure out how big a buffer we need to allocate
    long file_size;
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Could not allocate buffer for file '%s' (%li bytes)\n",
            filename, file_size);
        exit(1);
    }

    // Fill buffer
    size_t got = fread(buffer, 1, file_size, file);
    if(got < file_size){
        fprintf(stderr,
            "Could not read (all of) file '%s' (%li bytes): ",
            filename, file_size);
        perror(NULL);
        exit(1);
    }
    buffer[file_size] = '\0';

    // Close file & return!
    fclose(file);
    return buffer;
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
