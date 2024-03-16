#ifndef _FILE_IO_H_
#define _FILE_IO_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"
#include "./debug_print.h"

bool read_file(File* file_data) {
    FILE* file;

    if ((file = fopen(file_data -> file_path, "rb")) == NULL) {
        // Read the error from strerror
        error_print("unable to open the file: %s\n", file_data -> file_path);
        return TRUE;
    }

    fseek(file, 0, SEEK_END);
    file_data -> size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    file_data -> data = (unsigned char*) calloc(file_data -> size, sizeof(unsigned char));
    
    int err = 0;
    unsigned int read_bytes = fread(file_data -> data, sizeof(unsigned char), file_data -> size, file);
    if (read_bytes != file_data -> size) {
        error_print("read %u bytes instead of %u from %s", read_bytes, file_data -> size, file_data -> file_path);
        return TRUE;
    } else if ((err = ferror(file))) {
        error_print("an error occured while reading %s", file_data -> file_path);
        return TRUE;
    }

    debug_print("read %u bytes from %s\n", read_bytes, file_data -> file_path);

    fclose(file);

    return FALSE;
}

#endif _FILE_IO_H_