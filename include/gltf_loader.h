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
    struct Object* children;
    struct Object* parent;
    unsigned int children_count;
} Object;

void append_obj(Object* parent_obj, Object obj) {
    // realloc
    (parent_obj -> children)[parent_obj -> children_count] = obj;
    
    return;
}

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

    Object* objects = (Object*) calloc(1, sizeof(Object));
    unsigned int obj_count = 0;
    
    read_until(bit_stream, "\"", NULL);
    
    Object main_object = (Object) { .children = NULL, .objects_count = 0, .parent = objects, .value = NULL };
    main_object.identifier = (char*) calloc(150, sizeof(char));
    read_until(bit_stream, "\"", &(main_object.identifier));
    
    if (get_next_byte_uc(bit_stream) != ':') {
        deallocate_bit_stream(bit_stream);
        free(file_path);
        return; 
    }
    
    get_next_byte(bit_stream); // Skip blank space
    char object_type_identifier = get_next_byte_uc(bit_stream);
    if (object_type_identifier == '[') {
            main_object.obj_type = ARRAY;
            read_until(bit_stream, "\"{", NULL);
    } else if (object_type_identifier == '\"') {
            main_object.obj_type = STRING;
            main_object.value = calloc(350, sizeof(char));
            read_until(bit_stream, "\"", (char**) &(main_object.value));
    } else if ((object_type_identifier - 48) >= 0 && (object_type_identifier - 48) <= 9) {
            main_object.obj_type = NUMBER;
            main_object.value = calloc(350, sizeof(char));
            read_until(bit_stream, "\"", (char**) &(main_object.value));
            read_until(bit_stream, ",", NULL);
    } else if (object_type_identifier == '{') {
            main_object.obj_type = DICTIONARY;
    }

    

    debug_print(YELLOW, "object: %s, type: %s, value: %s\n", main_object.identifier, objs_types[main_object.obj_type], (char*) main_object.value);

    deallocate_bit_stream(bit_stream);
    free(file_path);

    return;
}

#endif //_GLTF_LOADER_H_