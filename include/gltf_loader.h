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

static int s_atoi(char* value) {
    if (value == NULL) return 0;
    return atoi(value);
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

    return;
}

static void read_dictionary(BitStream* bit_stream, Object* objects) {
    while (bit_stream -> current_byte != '}') {
        if (bit_stream -> error) return;
        read_identifier(bit_stream, objects);
        read_until(bit_stream, ",}", NULL);
    }
    return;
}

static void read_identifier(BitStream* bit_stream, Object* objects) {
    Object current_object = (Object) { .children = NULL, .children_count = 0, .parent = objects, .value = NULL };
    read_until(bit_stream, "\"", NULL);
    current_object.identifier = (char*) calloc(350, sizeof(char));
    read_until(bit_stream, "\"", (char**) &(current_object.identifier));

    current_object.obj_type = get_obj_type(bit_stream);

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

    for (unsigned int child_count = 0; child_count < object -> children_count; ++obj, ++child_count) {
        if (!strcmp(obj -> identifier, identifier)) {
            return obj;
        }
    }

    return NULL;
}

static Object* get_object_by_id(char* id, Object* main_object, bool print_warning) {
    Object* object = main_object;
    BitStream* bit_stream = allocate_bit_stream((unsigned char*) id, strlen(id) + 1, FALSE);

    while (bit_stream -> byte < bit_stream -> size - 1) {
        int index = -1;
        char* identifier = (char*) calloc(350, sizeof(char));
        read_until(bit_stream, "/[", &identifier);

        if (bit_stream -> current_byte == '[') {
            char* str_index = (char*) calloc(25, sizeof(char));
            read_until(bit_stream, "]", &str_index);
            get_next_byte(bit_stream); // Skip the '/' symbol
            index = atoi(str_index);
            free(str_index);
        }
        
        object = get_object_from_identifier(identifier, object);
        if (object == NULL) {
            if (print_warning) debug_print(CYAN, "object not found...\n");
            free(identifier);
            return NULL;
        } else if (index != -1) object = object -> children + index;
        
        free(identifier);
    }

    if (object -> value == NULL && object -> children_count == 0) {
        debug_print(CYAN, "invalid index\n");
        return NULL;
    }

    return object;
}

typedef struct Array {
    void** data;
    unsigned int count;
} Array;

#define GET_ELEMENT(type, arr, index) ((type) (((arr).data)[index]))

Array init_arr() {
    Array arr = (Array) { .count = 0 };
    arr.data = (void**) calloc(1, sizeof(void*));
    return arr;
}

void append_element(Array* arr, void* element) {
    arr -> data = (void**) realloc(arr -> data, sizeof(void*) * (arr -> count + 1));
    (arr -> data)[arr -> count] = element;
    (arr -> count)++;
    return;
}

void deallocate_arr(Array arr) {
    debug_print(YELLOW, "deallocating array...");
    free(arr.data);
    return;
}

typedef Array Vertices;
typedef Array Normals;
typedef Array TextureCoords;

typedef enum Topology { POINTS, LINES, LINE_LOOP, LINE_STRIP, TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN } Topology;

typedef struct Face {
    unsigned int* indices;
    Topology topology;
} Face;

typedef struct Mesh {
    Vertices vertices; // equivalent to the POSITION attribute of glTF meshes
    Normals normals;
    TextureCoords texture_coords;
    Face* faces;
    unsigned int faces_count;
    unsigned int material_index;
} Mesh;

typedef struct Node {
    Array meshes_indices;
    struct Node* childrens;
    unsigned int children_count;
} Node;

typedef struct RGB {
    unsigned char R;
    unsigned char G;
    unsigned char B;
} RGB;

typedef enum Filter { NEAREST = 9728, LINEAR, NEAREST_MIPMAP_NEAREST = 9984, LINEAR_MIPMAP_NEAREST, NEAREST_MIPMAP_LINEAR, LINEAR_MIPMAP_LINEAR } Filter;
typedef enum Wrap { CLAMP_TO_EDGE = 33071, MIRRORED_REPEAT = 33648, REPEAT = 10497 } Wrap;

typedef struct Texture {
    char* texture_path;
    Filter mag_filter;
    Filter min_filter;
    Wrap wrap_s;
    Wrap wrap_t;
} Texture;

typedef struct Material {
    RGB ambientColor;
    RGB diffuseColor;
    RGB specularColor;
    RGB emissiveColor;
    Texture texture;
    float shininess;
    float opacity;
} Material;

typedef struct Scene {
    Node root_node;
    Mesh* meshes;
    unsigned int meshes_count;
    Material* materials;
    unsigned int materials_count;
} Scene;

Node create_node(Object* nodes_obj, unsigned int node_index) {
    Node node = {0};
    Object* node_obj = nodes_obj -> children + node_index;

    // Decode other children
    Object* node_children = get_object_by_id("children", node_obj, FALSE);
    if (node_children != NULL) {
        node.children_count = node_children -> children_count;
        node.childrens = (Node*) calloc(node.children_count, sizeof(Node));
        for (unsigned int i = 0; i < node.children_count; ++i) {
            unsigned int child_index = atoi((char*) ((node_children -> children)[i].value));
            (node.childrens)[i] = create_node(nodes_obj, child_index);
        }
    } else {
        node.children_count = 0;
        node.childrens = NULL;
    }

    // Decode mesh
    Object* meshes = get_object_by_id("mesh", node_obj, FALSE);
    if (meshes != NULL) {
        node.meshes_indices = init_arr();
        unsigned int meshes_count = meshes -> children_count;
        for (unsigned int i = 0; i < meshes_count; ++i) {
            unsigned int mesh_index = atoi((char*) ((meshes -> children)[i].value));
            append_element(&(node.meshes_indices), &mesh_index);
        }
    } else {
        node.meshes_indices = (Array) { .count = 0, .data = NULL };
    }

    return node;
}

typedef enum BufferTarget {ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER} BufferTarget;

Array decode_buffer_views(Object main_obj, char* path) {
    Array buffer_views = init_arr();
    Array buffers = init_arr();

    // Store buffers
    Object* buffers_obj = get_object_by_id("buffers", &main_obj, TRUE);
    for (unsigned int i = 0; i < buffers_obj -> children_count; ++i) {
        char* uri = (char*) (get_object_by_id("uri", buffers_obj -> children + i, TRUE) -> value);
        unsigned int byte_length = atoi((char*) (get_object_by_id("byteLength", buffers_obj -> children + i, TRUE) -> value));
        
        File buffer_data = {0};
        buffer_data.file_path = (char*) calloc(350, sizeof(char));
        int len = snprintf(buffer_data.file_path, 350, "%s%s", path, uri);
        buffer_data.file_path = (char*) realloc(buffer_data.file_path, sizeof(char) * len);
        read_file(&buffer_data);
        
        BitStream* bit_stream = allocate_bit_stream(buffer_data.data, byte_length, TRUE);
        append_element(&buffers, (void*) bit_stream);
        deallocate_file(&buffer_data, TRUE);
    }

    // Store buffer views
    Object* buffer_views_obj = get_object_by_id("bufferViews", &main_obj, TRUE);
    for (unsigned int i = 0; i < buffer_views_obj -> children_count; ++i) {
        unsigned int buffer_index = atoi((char*)(get_object_by_id("buffer", buffer_views_obj -> children + i, TRUE) -> value));
        unsigned int byte_length = atoi((char*)(get_object_by_id("byteLength", buffer_views_obj -> children + i, TRUE) -> value));
        unsigned int byte_offset = s_atoi((char*)(get_object_by_id("byteOffset", buffer_views_obj -> children + i, TRUE) -> value));

        unsigned char* bit_stream_data = GET_ELEMENT(BitStream*, buffers, buffer_index) -> stream + byte_offset;
        BitStream* buffer_view_stream = allocate_bit_stream(bit_stream_data, byte_length, TRUE);
        append_element(&buffer_views, buffer_view_stream);
    }

    // Deallocate buffers
    for (unsigned int i = 0; i < buffers.count; ++i) {
        deallocate_bit_stream(GET_ELEMENT(BitStream*, buffers, i));
    }
    deallocate_arr(buffers);

    return buffer_views;
}

typedef enum DataType { SCALAR, VEC_2, VEC_3, VEC_4, MAT_2, MAT_3, MAT_4 } DataType;
unsigned char elements_count[] = { 1, 2, 3, 4, 4, 9, 16 };
typedef enum ComponentType { BYTE, UNSIGNED_BYTE, SHORT, UNSIGNED_SHORT, UNSIGNED_INT, FLOAT } ComponentType;
unsigned char byte_lengths[] = { sizeof(char), sizeof(unsigned char), sizeof(short int), sizeof(unsigned short int), sizeof(unsigned int), sizeof(float) };

typedef struct Accessor {
    void* data;
    ComponentType component_type;
    unsigned int elements_count;
    DataType data_type;
} Accessor;

DataType get_data_type(char* data_type_str) {
    if (!strcmp("SCALAR", data_type_str)) return SCALAR;
    else if (!strcmp("VEC_2", data_type_str)) return VEC_2;
    else if (!strcmp("VEC_3", data_type_str)) return VEC_3;
    else if (!strcmp("VEC_4", data_type_str)) return VEC_4;
    else if (!strcmp("MAT_2", data_type_str)) return MAT_2;
    else if (!strcmp("MAT_3", data_type_str)) return MAT_3;
    else if (!strcmp("MAT_4", data_type_str)) return MAT_4;
    else return SCALAR;
}

Array decode_accessors(Object main_obj, char* path) {
    Array accessors = init_arr();
    Array buffer_views = decode_buffer_views(main_obj, path);

    Object* accessors_obj = get_object_by_id("accessors", &main_obj, TRUE);
    for (unsigned int i = 0; i < accessors_obj -> children_count; ++i) {
        unsigned int buffer_view_index = atoi((char*) (get_object_by_id("bufferView", accessors_obj -> children + i, TRUE) -> value));
        unsigned int component_type = atoi((char*) (get_object_by_id("componentType", accessors_obj -> children + i, TRUE) -> value)) % 5120;
        unsigned int elements_count = atoi((char*) (get_object_by_id("count", accessors_obj -> children + i, TRUE) -> value));
        unsigned int byte_offset = s_atoi((char*)(get_object_by_id("byteOffset", accessors_obj -> children + i, TRUE) -> value));
        DataType data_type = get_data_type((char*) (get_object_by_id("type", accessors_obj -> children + i, TRUE) -> value));
        BitStream* buffer_view_stream = GET_ELEMENT(BitStream*, buffer_views, buffer_view_index);
        buffer_view_stream -> byte = byte_offset;

        Accessor* accessor = (Accessor*) calloc(1, sizeof(Accessor));
        *accessor = (Accessor) { .component_type = component_type, .elements_count = elements_count, .data_type = data_type };
        accessor -> data = get_next_n_byte(buffer_view_stream, elements_count, byte_lengths[component_type]);
        append_element(&accessors, (void*) accessor);
    }

    // Deallocate buffers
    for (unsigned int i = 0; i < buffer_views.count; ++i) {
        deallocate_bit_stream(GET_ELEMENT(BitStream*, buffer_views, i));
    }
    deallocate_arr(buffer_views);
    
    return accessors;
}

Array extract_elements(Accessor obj_accessor) {
    Array arr = init_arr();
    for (unsigned int s = 0; s < obj_accessor.elements_count; ++s) {
        unsigned char element_size = elements_count[obj_accessor.data_type];
        unsigned char byte_size = byte_lengths[obj_accessor.component_type];
        void* element = calloc(element_size, byte_size);
        unsigned char* data = ((unsigned char*) obj_accessor.data) + (s * element_size * byte_size);

        for (unsigned int t = 0; t < element_size; ++t) {
            if (byte_size == BYTE) ((char*) element)[t] = data[t * element_size * byte_size];
            else if (byte_size == UNSIGNED_BYTE) ((unsigned char*) element)[t] = data[t * element_size * byte_size];
            else if (byte_size == SHORT) ((short int*) element)[t] = (data[t * element_size * byte_size] << 8) + data[t * element_size * byte_size + 1];
            else if (byte_size == UNSIGNED_SHORT) ((unsigned short int*) element)[t] = (data[t * element_size * byte_size] << 8) + data[t * element_size * byte_size + 1];
            else if (byte_size == UNSIGNED_INT) ((unsigned int*) element)[t] = (data[t * element_size * byte_size] << 24) + (data[t * element_size * byte_size + 1] << 16) + (data[t * element_size * byte_size + 2] << 8) + data[t * element_size * byte_size + 3];
            else if (byte_size == FLOAT) {
                unsigned int value = (data[t * element_size * byte_size] << 24) + (data[t * element_size * byte_size + 1] << 16) + (data[t * element_size * byte_size + 2] << 8) + data[t * element_size * byte_size + 3];
                ((float*) element)[t] = *((float*) &value);
            }
        }
        
        append_element(&arr, element);
    }
    return arr;
}

unsigned char topology_size[] = { 1, 2, 2, 2, 3, 3, 3 };

Face* create_faces(Array indices_arr, Topology topology, unsigned int* faces_count) {
    Face* faces = (Face*) calloc(1, sizeof(Face)); 
    unsigned int total_faces = indices_arr.count;
    if (topology == TRIANGLE_STRIP || topology == TRIANGLE_FAN) total_faces -= 2;
    else if (topology == LINE_STRIP) total_faces -= 1;
    else if (topology == LINES) total_faces /= 2;
    else if (topology == TRIANGLES) total_faces /= 3;

    for (unsigned int i = 0; i < total_faces; ++i, ++(*faces_count)) {
        faces = (Face*) realloc(faces, sizeof(Face) * (*faces_count + 1));
        faces[i].topology = topology;
        faces[i].indices = (unsigned int*) calloc(topology_size[topology], sizeof(unsigned int));
        if (topology == LINE_STRIP) {
            faces[i].indices[0] = *GET_ELEMENT(unsigned int*, indices_arr, i);
            faces[i].indices[1] = *GET_ELEMENT(unsigned int*, indices_arr, i + 1);
        } else if (topology == LINE_LOOP) {
            faces[i].indices[0] = *GET_ELEMENT(unsigned int*, indices_arr, i);
            faces[i].indices[1] = *GET_ELEMENT(unsigned int*, indices_arr, (i + 1) % indices_arr.count);
        } else if (topology == LINES) {
            faces[i].indices[0] = *GET_ELEMENT(unsigned int*, indices_arr, 2 * i);
            faces[i].indices[1] = *GET_ELEMENT(unsigned int*, indices_arr, (2 * i) + 1);
        } else if (topology == TRIANGLES) {
            faces[i].indices[0] = *GET_ELEMENT(unsigned int*, indices_arr, 3 * i);
            faces[i].indices[1] = *GET_ELEMENT(unsigned int*, indices_arr, (3 * i) + 1);
            faces[i].indices[2] = *GET_ELEMENT(unsigned int*, indices_arr, (3 * i) + 2);
        } else if (topology == TRIANGLE_STRIP) {
            faces[i].indices[0] = *GET_ELEMENT(unsigned int*, indices_arr, i);
            faces[i].indices[1] = *GET_ELEMENT(unsigned int*, indices_arr, i + (1 + i % 2));
            faces[i].indices[2] = *GET_ELEMENT(unsigned int*, indices_arr, i + (2 - i % 2));
        } else if (topology == TRIANGLE_FAN) {
            faces[i].indices[0] = *GET_ELEMENT(unsigned int*, indices_arr, i + 1);
            faces[i].indices[1] = *GET_ELEMENT(unsigned int*, indices_arr, i + 2);
            faces[i].indices[2] = *GET_ELEMENT(unsigned int*, indices_arr, 0);
        } else if (topology == POINTS) faces[i].indices[0] = *GET_ELEMENT(unsigned int*, indices_arr, i);
    }

    return faces;
}

Mesh* decode_mesh(Array accessors, Object main_obj, unsigned int* meshes_count) {
    Mesh* meshes = (Mesh*) calloc(1, sizeof(Mesh));
    Object* meshes_obj = get_object_by_id("meshes", &main_obj, TRUE);
    for (unsigned int i = 0; i < meshes_obj -> children_count; ++i, ++(*meshes_count)) {
        meshes = (Mesh*) realloc(meshes, sizeof(Mesh) * (*meshes_count + 1));
        Object* primitives = get_object_by_id("primitives", meshes_obj -> children + i, TRUE);
        for (unsigned int j = 0; j < primitives -> children_count; ++j) {
            unsigned int material_index = atoi((char*) (get_object_by_id("material", primitives -> children + j, TRUE) -> value));
            Topology topology = atoi((char*) (get_object_by_id("mode", primitives -> children + j, TRUE) -> value));
            unsigned int indices_index = atoi((char*) (get_object_by_id("indices", primitives -> children + j, TRUE) -> value));
            unsigned int vertices_index = atoi((char*) (get_object_by_id("attributes/POSITION", primitives -> children + j, TRUE) -> value));
            unsigned int normal_index = atoi((char*) (get_object_by_id("attributes/NORMAL", primitives -> children + j, TRUE) -> value));
            unsigned int tex_coords_index = atoi((char*) (get_object_by_id("attributes/TEXCOORD_0", primitives -> children + j, TRUE) -> value));

            Accessor* vertex_accessor = GET_ELEMENT(Accessor*, accessors, vertices_index);
            meshes[i].vertices = extract_elements(*vertex_accessor);            
            Accessor* normal_accessor = GET_ELEMENT(Accessor*, accessors, normal_index);
            meshes[i].normals = extract_elements(*normal_accessor);            
            Accessor* tex_coords_accessor = GET_ELEMENT(Accessor*, accessors, tex_coords_index);
            meshes[i].texture_coords = extract_elements(*tex_coords_accessor);
            Accessor* indices_accessor = GET_ELEMENT(Accessor*, accessors, indices_index);
            Array indices_arr = extract_elements(*indices_accessor);
            meshes[i].faces_count = 0;
            meshes[i].faces = create_faces(indices_arr, topology, &(meshes[i].faces_count));
            meshes[i].material_index = material_index;
        }
    }

    return meshes;
}

Texture* collect_textures(Object main_obj, unsigned int* texture_count, char* path) {
    Texture* textures = (Texture*) calloc(1, sizeof(Texture));
    Object* textures_obj = get_object_by_id("textures", &main_obj, TRUE);
    Object* sampler_obj = get_object_by_id("samplers", &main_obj, TRUE);
    Object* images_obj = get_object_by_id("images", &main_obj, TRUE);
    
    for (unsigned int i = 0; i < textures_obj ->children_count; ++i, ++(*texture_count)) {
        unsigned int sampler_id = atoi((char*) (get_object_by_id("sampler", textures_obj -> children + i, TRUE) -> value));
        unsigned int source_id = atoi((char*) (get_object_by_id("source", textures_obj -> children + i, TRUE) -> value));
        textures[i].mag_filter = atoi((char*) (get_object_by_id("magFilter", sampler_obj -> children + sampler_id, TRUE) -> value));
        textures[i].min_filter = atoi((char*) (get_object_by_id("minFilter", sampler_obj -> children + sampler_id, TRUE) -> value));
        textures[i].wrap_s = atoi((char*) (get_object_by_id("wrapS", sampler_obj -> children + sampler_id, TRUE) -> value));
        textures[i].wrap_t = atoi((char*) (get_object_by_id("wrapT", sampler_obj -> children + sampler_id, TRUE) -> value));
        textures[i].texture_path = (char*) calloc(350, sizeof(char));
        int path_len = snprintf(textures[i].texture_path, 350, "%s%s", path, (char*) (get_object_by_id("uri", images_obj -> children + source_id, TRUE) -> value));
        textures[i].texture_path = (char*) realloc(textures[i].texture_path, sizeof(char) * path_len);
    }
    
    return textures;
}

Material* decode_materials(Object main_obj, unsigned int* materials_count, char* path) {
    Material* materials = (Material*) calloc(1, sizeof(Material));

    unsigned int texture_count = 0;
    Texture* textures = collect_textures(main_obj, &texture_count, path);
    

    return materials;
}

Scene decode_scene(Object main_obj, char* path) {
    Scene scene = {0};

    Array accessors = decode_accessors(main_obj, path);

    unsigned int root_node_index = atoi((char*) (get_object_by_id("scenes[0]/nodes[0]", &main_obj, TRUE) -> value));
    debug_print(WHITE, "root node: %u\n", root_node_index);

    Object* nodes_obj = get_object_by_id("nodes", &main_obj, TRUE);
    scene.root_node = create_node(nodes_obj, root_node_index);

    debug_print(WHITE, "root node: children count: %u, meshes_count: %u\n", scene.root_node.children_count, scene.root_node.meshes_indices.count);

    // decode meshes
    scene.meshes_count = 0;
    scene.meshes = decode_mesh(accessors, main_obj, &scene.meshes_count);

    // deallocate accessors
    for (unsigned int i = 0; i < accessors.count; ++i) {
        free(GET_ELEMENT(Accessor*, accessors, i) -> data);
        free(GET_ELEMENT(Accessor*, accessors, i));
    }
    deallocate_arr(accessors);

    // decode materials
    scene.materials_count = 0;
    scene.materials = decode_materials(main_obj, &scene.materials_count, path);
    
    return scene;
}

void decode_gltf(char* path) {
    char* file_path = (char*) calloc(175, sizeof(char));
    int len = snprintf(file_path, 175, "%sscene.gltf", path);
    file_path = (char*) realloc(file_path, sizeof(char) * len);

    File file_data = (File) {.file_path = file_path};
    read_file(&file_data);

    BitStream* bit_stream = allocate_bit_stream(file_data.data, file_data.size, FALSE);
    deallocate_file(&file_data, FALSE);

    if (get_next_bytes_us(bit_stream) != (unsigned short int)(('{' << 8) | '\n')) {
        deallocate_bit_stream(bit_stream);
        return; 
    }

    Object default_object = (Object) { .children = calloc(1, sizeof(Object)), .children_count = 0, .parent = NULL, .value = NULL, .identifier = "main", .obj_type = DICTIONARY };
    read_dictionary(bit_stream, &default_object);
    deallocate_bit_stream(bit_stream);

    Scene scene = decode_scene(default_object, path);
    (void)scene;

    return;
}

#endif //_GLTF_LOADER_H_