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

static void append_obj(Object* parent_obj, Object obj) {
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

    while ((*str)[len] == ' ' || (*str)[len] == '\n' || (*str)[len] == '\r') { len--; }
    
    len++; // skip to the position of the last whitespace

    (*str)[len] = '\0';

    len++;

    *str = (char*) realloc(*str, sizeof(char) * (len + 1));
    
    unsigned int ind = 0;
    for (ind = 0; ind < len && ((*str)[ind] == ' ' || (*str)[ind] == '\n' || (*str)[len] == '\r'); ++ind) { }
    
    char* new_str = (char*) calloc(len - ind, sizeof(char));

    for (unsigned int i = 0; ind < len; ++i, ++ind) {
        new_str[i] = (*str)[ind];
    }

    free(*str);
    *str = new_str;

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
            if (bit_stream -> error) return;
            Object child_obj = (Object) { .children = NULL, .children_count = 0, .parent = objects, .identifier = NULL, .obj_type = collection_type };
            child_obj.value = calloc(350, sizeof(char));
            read_until(bit_stream, ",]", (char**) &(child_obj.value));
            strip((char**) &(child_obj.value));
            append_obj(objects, child_obj);
        }
    } else if (collection_type == DICTIONARY || collection_type == ARRAY) {
        while (bit_stream -> current_byte != ']') {
            if (bit_stream -> error) return;
            char* child = (char*) calloc(350, sizeof(char));
            int len = snprintf(child, 350, "%s child-%u", objects -> identifier, objects -> children_count);
            child = (char*) realloc(child, sizeof(char) * (len + 1));
            Object child_obj = (Object) { .children = NULL, .children_count = 0, .parent = objects, .identifier = child, .obj_type = collection_type };
            if (collection_type == DICTIONARY) read_dictionary(bit_stream, &child_obj);
            else read_array(bit_stream, &child_obj);
            read_until(bit_stream, ",]", NULL);
            append_obj(objects, child_obj);
        }
    } else {
        error_print("Invalid object type: %s.\n", objs_types[collection_type]);
    }

    debug_print(YELLOW, "'%s' now contains %u elements\n", objects -> identifier, objects -> children_count);

    return;
}

static void read_dictionary(BitStream* bit_stream, Object* objects) {
    while (bit_stream -> current_byte != '}') {
        if (bit_stream -> error) return;
        read_identifier(bit_stream, objects);
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
        skip_back(bit_stream, 1);
    } else if (current_object.obj_type == NUMBER) {
        current_object.value = calloc(350, sizeof(char));
        read_until(bit_stream, ",\n", (char**) &(current_object.value));
        strip((char**) &(current_object.value));
        skip_back(bit_stream, 1);
    }

    append_obj(objects, current_object);

    return;
}

static Object* get_object_from_identifier(char* identifier, Object* object) {
    Object* obj = object -> children;
    debug_print(YELLOW, "children count: %u\n", object -> children_count);
    unsigned int child_count = 0;

    while (strcmp(obj -> identifier, identifier) && (child_count < object -> children_count)) {
        obj++;
        child_count++;
    }

    if (child_count == object -> children_count) {
        return NULL;
    }

    debug_print(YELLOW, "child_count: %u out of %u\n", child_count, object -> children_count);

    return obj;
}

static Object* get_object_by_id(char* id, Object* main_object) {
    Object* object = main_object;
    BitStream* bit_stream = allocate_bit_stream((unsigned char*) id, strlen(id) + 1);

    while (bit_stream -> byte < bit_stream -> size - 1) {
        int index = -1;
        char* identifier = (char*) calloc(350, sizeof(char));
        read_until(bit_stream, "/[", &identifier);
        debug_print(YELLOW, "identifier: '%s', finding: '%s'\n", object -> identifier, identifier);

        if (bit_stream -> current_byte == '[') {
            char* str_index = (char*) calloc(25, sizeof(char));
            read_until(bit_stream, "]", &str_index);
            get_next_byte(bit_stream); // Skip the '/' symbol
            index = atoi(str_index);
            free(str_index);
        }
        
        object = get_object_from_identifier(identifier, object);
        if (index != -1) object = object -> children + index;
        if (object == NULL) {
            debug_print(CYAN, "object not found...\n");
            free(identifier);
            return NULL;
        }

        
        free(identifier);
    }

    if (object -> value == NULL) {
        debug_print(CYAN, "invalid index\n");
        return NULL;
    }

    return object;
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

    Object* obj = get_object_by_id("accessors[0]/max[2]", &default_object);
    
    if (obj == NULL) {
        return;
    }

    debug_print(WHITE, "value: '%s', type: %s\n", (char*) obj -> value, objs_types[obj -> obj_type]);

    return;
}

#endif //_GLTF_LOADER_H_