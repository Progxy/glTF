#ifndef _DECODER_H_
#define _DECODER_H_

#include "./debug_print.h"
#include "./gltf_loader.h"

void decode_model(char* file_path) {
    debug_print(YELLOW, "Loading model from %s...\n", file_path);
    decode_gltf(file_path);
    return;
}

#endif //_DECODER_H_