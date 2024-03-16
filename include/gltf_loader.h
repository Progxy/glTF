#ifndef _GLTF_LOADER_H_
#define _GLTF_LOADER_H_

#include "./debug_print.h"
#include "./bitstream.h"
#include "./file_io.h"

typedef enum ObjectType { ARRAY, STRING, NUMBER, DICTIONARY } ObjectType;
const char* objs_types[] = {"ARRAY", "STRING", "NUMBER", "DICTIONARY"};

typedef struct Object {
    char* identifier;
    void* value;
    ObjectType obj_type;
    struct Object* objects;
    unsigned int objects_count;
} Object;

void decode_gltf(char* path) {
    char* file_path = (char*) calloc(175, sizeof(char));
    int len = snprintf(file_path, 175, "%s/scene.gltf", path);
    file_path = (char*) realloc(file_path, sizeof(char) * len);

    File file_data = (File) {.file_path = file_path};
    read_file(&file_data);

    BitStream* bit_stream = allocate_bit_stream(file_data.data, file_data.size);

    if (get_next_bytes_us(bit_stream) != (unsigned short int)(('{' << 8) | '\n')) {
        deallocate_bit_stream(bit_stream);
        free(file_path);
        return; 
    }
    
    read_until(bit_stream, '\"', NULL);
    
    char* object = (char*) calloc(150, sizeof(char));
    read_until(bit_stream, '\"', &object);
    
    if (get_next_byte_uc(bit_stream) != ':') {
        deallocate_bit_stream(bit_stream);
        free(file_path);
        return; 
    }
    
    get_next_byte(bit_stream); // Skip blank space
    char object_identifier = get_next_byte_uc(bit_stream);
    ObjectType obj_type;
    if (object_identifier == '[') obj_type = ARRAY;
    else if (object_identifier == '\"') obj_type = STRING;
    else if ((object_identifier - 48) >= 0 && (object_identifier - 48) <= 9) obj_type = NUMBER;
    else if (object_identifier == '{') obj_type = DICTIONARY;

    debug_print(YELLOW, "object: %s, type: %s\n", object, objs_types[obj_type]);

    free(object);

    deallocate_bit_stream(bit_stream);
    free(file_path);

    return;
}

#endif //_GLTF_LOADER_H_