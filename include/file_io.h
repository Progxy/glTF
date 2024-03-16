#ifndef _FILE_IO_H_
#define _FILE_IO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./types.h"
#include "./debug_print.h"

bool read_file(File* file_data) {
    FILE* file;

    int err = 0;
    if ((file = fopen(file_data -> file_path, "rb")) == NULL) {
        err = ferror(file);
        error_print("unable to open the file: %s, cause: %s\n", file_data -> file_path, strerror(err));
        return TRUE;
    }

    fseek(file, 0, SEEK_END);
    file_data -> size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    file_data -> data = (unsigned char*) calloc(file_data -> size, sizeof(unsigned char));
    
    unsigned int read_bytes = fread(file_data -> data, sizeof(unsigned char), file_data -> size, file);
    if (read_bytes != file_data -> size) {
        error_print("read %u bytes instead of %u from %s\n", read_bytes, file_data -> size, file_data -> file_path);
        return TRUE;
    } else if ((err = ferror(file))) {
        error_print("an error occured while reading %s, cause: %s\n", file_data -> file_path, strerror(err));
        return TRUE;
    }

    debug_print("read %u bytes from %s\n", read_bytes, file_data -> file_path);

    fclose(file);

    return FALSE;
}

#endif _FILE_IO_H_