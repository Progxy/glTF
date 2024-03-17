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

#define STR_LEN(str, len) while (str[len] != '\0') { len++; }

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

    str[len] = '\0';

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
            break;
        }
        
        default: {
            obj_type = INVALID_OBJECT;
            break;
        }
    }

    // skip back 
    (bit_stream -> byte)--;

    return obj_type;
}

static void read_array(BitStream* bit_stream, Object* objects) {
    // We can find a collection of dictionary, numbers or strings

    ObjectType collection_type = get_obj_type(bit_stream);
    debug_print(YELLOW, "found a collection of %s\n", objs_types[collection_type]);

    if (collection_type == NUMBER || collection_type == STRING) {
        while (bit_stream -> current_byte != ']') {
            Object child_obj = (Object) { .children = NULL, .children_count = 0, .parent = objects, .identifier = (char*) calloc(350, sizeof(char)), .value = NULL, .obj_type = collection_type };
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

    return;
}

void read_identifier(BitStream* bit_stream, Object* objects);

static void read_dictionary(BitStream* bit_stream, Object* objects) {
    while (bit_stream -> current_byte != "}") {
        read_identifier(bit_stream, objects);
        read_until(bit_stream, ",}", NULL);
    }
    return;
}

void read_identifier(BitStream* bit_stream, Object* objects) {
    read_until(bit_stream, "\"", NULL);
    
    Object current_object = (Object) { .children = NULL, .children_count = 0, .parent = objects, .identifier = (char*) calloc(350, sizeof(char)), .value = NULL };
    
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

    //Object* objects = NULL;
    Object default_object = (Object) { .children = calloc(1, sizeof(Object)), .children_count = 0, .parent = NULL, .value = NULL, .identifier = "main", .obj_type = DICTIONARY };
    //objects = &default_object;

    read_identifier(bit_stream, &default_object);

    // while (TRUE) {
    //     if (get_next_byte_uc(bit_stream) == '}' && objects -> parent == NULL) {
    //         break;
    //     } else if (bit_stream -> error) {
    //         error_print("%s\n", err_codes[bit_stream -> error]);
    //         break;
    //     } else {
    //         (bit_stream -> byte)--;
    //     }

    //     read_until(bit_stream, "\"", NULL);
    //     Object current_object = (Object) { .children = NULL, .children_count = 0, .parent = objects, .value = NULL };
    //     current_object.identifier = (char*) calloc(150, sizeof(char));
    //     read_until(bit_stream, "\"", &(current_object.identifier));

    //     read_until(bit_stream, "[\"0123456789{", NULL);

    //     char object_type_identifier = bit_stream -> current_byte;
    //     if (object_type_identifier == '[') {
    //         current_object.obj_type = ARRAY;
    //         debug_print(YELLOW, "decoding %s, identifier: '%s'...\n", objs_types[current_object.obj_type], current_object.identifier);
    //         objects = &current_object;
    //         read_until(bit_stream, "{-0123456789", NULL);

    //         if (bit_stream -> current_byte == '{') {
    //             Object dict_object = (Object) { .children = calloc(1, sizeof(Object)), .parent = objects, .children_count = 0, .identifier = "", .obj_type = DICTIONARY, .value = NULL };
    //             append_obj(objects, dict_object);
    //             objects = &dict_object;
    //             debug_print(YELLOW, "decoding %s...\n", objs_types[dict_object.obj_type]);
    //         } else {
    //             (bit_stream -> byte)--;
    //             while (TRUE) {
    //                 Object num_obj = (Object) { .children = NULL, .children_count = 0, .parent = objects, .value = calloc(350, sizeof(char)), .obj_type = NUMBER, .identifier = "" };
    //                 debug_print(YELLOW, "decoding %s...\n", objs_types[num_obj.obj_type]);
    //                 read_until(bit_stream, ",]", (char**) &(num_obj.value));
    //                 append_obj(objects, num_obj);
    //                 if (bit_stream -> current_byte == ']') {
    //                     objects = objects -> parent;
    //                     break;
    //                 }
    //             }
    //         }
    //     } else if (object_type_identifier == '\"') {
    //         current_object.obj_type = STRING;
    //         debug_print(YELLOW, "decoding %s, identifier: '%s'...\n", objs_types[current_object.obj_type], current_object.identifier);

    //         current_object.value = calloc(350, sizeof(char));

    //         read_until(bit_stream, "\"", (char**) &(current_object.value));
    //         debug_print(YELLOW, "value: '%s'\n", (char*) current_object.value);
    //         append_obj(objects, current_object);
    //         read_until(bit_stream, "\"}]", NULL);

    //         char terminator = bit_stream -> current_byte;
    //         unsigned int old_pos = bit_stream -> byte;
    //         if (terminator == '}' || terminator == ']') {
    //             objects = (objects -> parent != NULL) ? objects -> parent : objects;
    //             debug_print(YELLOW, "terminator '%c' found, switching back to object parent\n", (char) bit_stream -> current_byte);
    //             read_until(bit_stream, ",}]", NULL);
    //             if (bit_stream -> current_byte == ',' && (bit_stream -> byte == old_pos + 1)) {
    //                 read_until(bit_stream, "\"[{", NULL);
    //                 if (bit_stream -> current_byte == '{' || bit_stream -> current_byte == '[') {
    //                     Object dict_object = (Object) { .children = calloc(1, sizeof(Object)), .parent = objects, .children_count = 0, .identifier = "", .obj_type = terminator == '}' ? DICTIONARY : ARRAY, .value = NULL };
    //                     append_obj(objects, dict_object);
    //                     objects = &dict_object;
    //                     debug_print(YELLOW, "decoding %s...\n", objs_types[dict_object.obj_type]);
    //                 }
    //             } else if (bit_stream -> current_byte == ']' || bit_stream -> current_byte == '}') {
    //                 objects = (objects -> parent != NULL) ? objects -> parent : objects;
    //                 debug_print(YELLOW, "terminator '%c' found, switching back to object parent\n", (char) bit_stream -> current_byte);
    //             }
    //         }

    //         (bit_stream -> byte)--; // Skip back '\"'
    //     } else if (((object_type_identifier - 48) >= 0 && (object_type_identifier - 48) <= 9) || object_type_identifier == '-') {
    //         current_object.obj_type = NUMBER;
    //         debug_print(YELLOW, "decoding %s, identifier: '%s'...\n", objs_types[current_object.obj_type], current_object.identifier);
    //         current_object.value = calloc(350, sizeof(char));

    //         read_until(bit_stream, ",", (char**) &(current_object.value));
    //         append_obj(objects, current_object);
    //         read_until(bit_stream, "\"}]", NULL);

    //         char terminator = bit_stream -> current_byte;
    //         unsigned int old_pos = bit_stream -> byte;
    //         if (terminator == '}' || terminator == ']') {
    //             objects = (objects -> parent != NULL) ? objects -> parent : objects;
    //             debug_print(YELLOW, "terminator '%c' found, switching back to object parent\n", (char) bit_stream -> current_byte);
    //             read_until(bit_stream, ",}]", NULL);
    //             if (bit_stream -> current_byte == ',' && (bit_stream -> byte == old_pos + 1)) {
    //                 read_until(bit_stream, "\"[{", NULL);
    //                 if (bit_stream -> current_byte == '{' || bit_stream -> current_byte == '[') {
    //                     Object dict_object = (Object) { .children = calloc(1, sizeof(Object)), .parent = objects, .children_count = 0, .identifier = "", .obj_type = terminator == '}' ? DICTIONARY : ARRAY, .value = NULL };
    //                     append_obj(objects, dict_object);
    //                     objects = &dict_object;
    //                     debug_print(YELLOW, "decoding %s...\n", objs_types[dict_object.obj_type]);
    //                 }
    //             } else if (bit_stream -> current_byte == ']' || bit_stream -> current_byte == '}') {
    //                 objects = (objects -> parent != NULL) ? objects -> parent : objects;
    //                 debug_print(YELLOW, "terminator '%c' found, switching back to object parent\n", (char) bit_stream -> current_byte);
    //             }
    //         }

    //         (bit_stream -> byte)--; // Skip back '\"'

    //     } else if (object_type_identifier == '{') {
    //         current_object.obj_type = DICTIONARY;
    //         debug_print(YELLOW, "decoding %s, identifier: '%s'...\n", objs_types[current_object.obj_type], current_object.identifier);
    //         objects = &current_object;
    //     }
    // }

    debug_print(YELLOW, "the main objects has %u children objects\n", objects -> children_count);

    deallocate_bit_stream(bit_stream);
    free(file_path);

    return;
}

#endif //_GLTF_LOADER_H_