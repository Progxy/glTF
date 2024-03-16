#include <stdio.h>
#include <stdlib.h>
#include "./include/decoder.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        error_print("Usage: ./example path_to_assets\n");
        return 1;
    }

    decode_model(argv[1]);
    
    return 0;
}