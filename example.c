#include <stdio.h>
#include <stdlib.h>
#include "./include/gltf_loader.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        error_print("Usage: ./example path_to_assets\n");
        return 1;
    }

    char* file_path = argv[1];
    debug_print(YELLOW, "Loading model from %s...\n", file_path);
    decode_gltf(file_path);
    
    return 0;
}