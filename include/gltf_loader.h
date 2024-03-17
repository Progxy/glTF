#ifndef _GLTF_LOADER_H_
#define _GLTF_LOADER_H_

#include "./debug_print.h"
#include "./bitstream.h"
#include "./file_io.h"

typedef enum ObjectType { ARRAY, STRING, NUMBER, DICTIONARY, INVALID_OBJECT } ObjectType;
const char* objs_types[] = {"ARRAY", "STRING", "NUMBER", "DICTIONARY", "INVALID_OBJECT"};

typedef struct Object {
    char* identifier;
    void* value;
    ObjectType obj_type;
    struct Object* children;
    struct Object* parent;
    unsigned int children_count;
} Object;

#define STR_LEN(str, len) while ((str)[len] != '\0') { len++; }

void append_obj(Object* parent_obj, Object obj) {
    parent_obj -> children = (Object*) realloc(parent_obj -> children, sizeof(Object) * (parent_obj -> children_count + 1));
    (parent_obj -> children)[parent_obj -> children_count] = obj;
    (parent_obj -> children)[parent_obj -> children_count].parent = parent_obj;
    (parent_obj -> children_count)++;
    return;
}

static void strip(char** str) {
    unsigned int len = 0;
    STR_LEN(*str, len);

    len--; // skip the null terminator

    while ((*str)[len] == ' ') { len--; }
    
    len++; // skip to the position of the last whitespace

    (*str)[len] = '\0';

    *str = (char*) realloc(*str, sizeof(char) * (len + 1));

    return;
}

static ObjectType get_obj_type(BitStream* bit_stream) {
    ObjectType obj_type;

    read_until(bit_stream, "\"-0123456789{[", NULL);

    switch (bit_stream -> current_byte) {
        case '\"': {
            obj_type = STRING;
            break;
        }

        case '{': {
            obj_type = DICTIONARY;
            break;
        }

        case '[': {
            obj_type = ARRAY;
            break;
        }

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            obj_type = NUMBER;
            skip_back(bit_stream, 2);
            break;
        }
        
        default: {
            obj_type = INVALID_OBJECT;
            break;
        }
    }


    return obj_type;
}

static void read_identifier(BitStream* bit_stream, Object* objects);
static void read_dictionary(BitStream* bit_stream, Object* objects);

static void read_array(BitStream* bit_stream, Object* objects) {
    // We can find a collection of dictionary, numbers or strings
    ObjectType collection_type = get_obj_type(bit_stream);
    debug_print(YELLOW, "found a collection of %s\n", objs_types[collection_type]);

    if (collection_type == NUMBER || collection_type == STRING) {
        while (bit_stream -> current_byte != ']') {
            Object child_obj = (Object) { .children = NULL, .children_count = 0, .parent = objects, .identifier = NULL, .obj_type = collection_type };
            child_obj.value = calloc(350, sizeof(char));
            read_until(bit_stream, ",]", (char**) &(child_obj.value));
            strip((char**) &(child_obj.value));
            append_obj(objects, child_obj);
        }
    } else if (collection_type == DICTIONARY || collection_type == ARRAY) {
        while (bit_stream -> current_byte != ']') {
            if (collection_type == DICTIONARY) read_dictionary(bit_stream, objects);
            else read_array(bit_stream, objects);
            read_until(bit_stream, ",]", NULL);
        }
    } else {
        error_print("Invalid object type: %s.\n", objs_types[collection_type]);
    }

    debug_print(YELLOW, "'%s' now contains %u elements\n", objects -> identifier, objects -> children_count);

    return;
}

static void read_dictionary(BitStream* bit_stream, Object* objects) {
    while (bit_stream -> current_byte != '}') {
        read_identifier(bit_stream, objects);
        skip_back(bit_stream, 1); // Skip back to , if there's
        read_until(bit_stream, ",}", NULL);
    }
    debug_print(YELLOW, "'%s' now contains %u elements\n", objects -> identifier, objects -> children_count);
    return;
}

static void read_identifier(BitStream* bit_stream, Object* objects) {
    Object current_object = (Object) { .children = NULL, .children_count = 0, .parent = objects, .value = NULL };
    read_until(bit_stream, "\"", NULL);
    current_object.identifier = (char*) calloc(350, sizeof(char));
    read_until(bit_stream, "\"", (char**) &(current_object.identifier));

    current_object.obj_type = get_obj_type(bit_stream);
    debug_print(YELLOW, "found object: '%s', type: %s\n", current_object.identifier, objs_types[current_object.obj_type]);

    if (current_object.obj_type == ARRAY) {
        current_object.children = (Object*) calloc(1, sizeof(Object));
        read_array(bit_stream, &current_object);
    } else if (current_object.obj_type == DICTIONARY) {
        current_object.children = (Object*) calloc(1, sizeof(Object));
        read_dictionary(bit_stream, &current_object);
    } else if (current_object.obj_type == STRING) {
        current_object.value = calloc(350, sizeof(char));
        read_until(bit_stream, "\"", (char**) &(current_object.value));
    } else if (current_object.obj_type == NUMBER) {
        current_object.value = calloc(350, sizeof(char));
        read_until(bit_stream, ",", (char**) &(current_object.value));
    }

    append_obj(objects, current_object);

    return;
}

void decode_gltf(char* path) {
    char* file_path = (char*) calloc(175, sizeof(char));
    int len = snprintf(file_path, 175, "%sscene.gltf", path);
    file_path = (char*) realloc(file_path, sizeof(char) * len);

    File file_data = (File) {.file_path = file_path};
    read_file(&file_data);

    BitStream* bit_stream = allocate_bit_stream(file_data.data, file_data.size);

    if (get_next_bytes_us(bit_stream) != (unsigned short int)(('{' << 8) | '\n')) {
        deallocate_bit_stream(bit_stream);
        free(file_path);
        return; 
    }

    Object default_object = (Object) { .children = calloc(1, sizeof(Object)), .children_count = 0, .parent = NULL, .value = NULL, .identifier = "main", .obj_type = DICTIONARY };
    read_dictionary(bit_stream, &default_object);
    deallocate_bit_stream(bit_stream);
    free(file_path);

    return;
}

#endif //_GLTF_LOADER_H_