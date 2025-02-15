#ifndef _TYPES_H_
#define _TYPES_H_

typedef unsigned char bool;

typedef enum BitStreamError {NO_ERROR, EXCEEDED_LENGTH} BitStreamError; 
typedef enum Filter { NEAREST = 9728, LINEAR, NEAREST_MIPMAP_NEAREST = 9984, LINEAR_MIPMAP_NEAREST, NEAREST_MIPMAP_LINEAR, LINEAR_MIPMAP_LINEAR } Filter;
typedef enum Topology { POINTS, LINES, LINE_LOOP, LINE_STRIP, TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN } Topology;
typedef enum ComponentType { BYTE, UNSIGNED_BYTE, SHORT, UNSIGNED_SHORT, UNSIGNED_INT = 5, FLOAT } ComponentType;
typedef enum Wrap { CLAMP_TO_EDGE = 33071, MIRRORED_REPEAT = 33648, REPEAT = 10497 } Wrap;
typedef enum ObjectType { ARRAY, STRING, NUMBER, DICTIONARY, INVALID_OBJECT } ObjectType;
typedef enum DataType { SCALAR, VEC2, VEC3, VEC4, MAT2, MAT3, MAT4 } DataType;
typedef enum Colors {RED = 31, GREEN, YELLOW, BLUE, PURPLE, CYAN, WHITE} Colors;
typedef enum BufferTarget {ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER} BufferTarget;

unsigned char byte_lengths[] = { sizeof(char), sizeof(unsigned char), sizeof(short int), sizeof(unsigned short int), 0, sizeof(unsigned int), sizeof(float) };
const char* objs_types[] = {"ARRAY", "STRING", "NUMBER", "DICTIONARY", "INVALID_OBJECT"};
unsigned char elements_count[] = { 1, 2, 3, 4, 4, 9, 16 };
unsigned char topology_size[] = { 1, 2, 2, 2, 3, 3, 3 };

typedef struct File {
    unsigned char* data;
    char* file_path;
    unsigned int size;
} File;

typedef struct BitStream {
    unsigned char* stream;
    unsigned int byte;
    unsigned char bit;
    unsigned int size;
    unsigned char current_byte;
    BitStreamError error;
} BitStream;

typedef struct Object {
    char* identifier;
    void* value;
    ObjectType obj_type;
    struct Object* children;
    struct Object* parent;
    unsigned int children_count;
} Object;

typedef struct Array {
    void** data;
    unsigned int count;
} Array;

typedef struct ArrayExtended {
    Array arr;
    DataType data_type;
    ComponentType component_type;
} ArrayExtended;

typedef ArrayExtended Vertices;
typedef ArrayExtended Normals;
typedef ArrayExtended Tangents;
typedef ArrayExtended TextureCoords;

typedef struct Face {
    unsigned int* indices;
    Topology topology;
} Face;

typedef struct Mesh {
    Vertices vertices; // equivalent to the POSITION attribute of glTF meshes
    Normals normals;
    Tangents tangents;
    TextureCoords texture_coords;
    Face* faces;
    unsigned int faces_count;
    unsigned int material_index;
} Mesh;

typedef struct Node {
    Array meshes_indices;
    struct Node* childrens;
    unsigned int children_count;
    float transformation_matrix[16];
    float translation_vec[3];
    float rotation_quat[4];\
    float scale_vec[3];
} Node;

typedef struct Texture {
    char* texture_path;
    Filter mag_filter;
    Filter min_filter;
    Wrap wrap_s;
    Wrap wrap_t;
    unsigned int tex_coord;
} Texture;

typedef struct PbrMetallicRoughness {
    float* base_color_factor;
    Texture base_color_texture;
    Texture metallic_roughness_texture;
    float metallic_factor;
    float roughness_factor;
} PbrMetallicRoughness;

typedef struct NormalTextureInfo { 
    Texture texture;
    unsigned int scale;
} NormalTextureInfo;

typedef struct OcclusionTextureInfo { 
    Texture texture;
    unsigned int strength;
} OcclusionTextureInfo;

typedef struct Material {
    PbrMetallicRoughness pbr_metallic_roughness;
    NormalTextureInfo normal_texture;
    OcclusionTextureInfo occlusion_texture;
    Texture emissive_texture;
    float* emissive_factor;
    char* alpha_mode;
    float alpha_cutoff;
    bool double_sided;
} Material;

typedef struct Scene {
    Node root_node;
    Mesh* meshes;
    unsigned int meshes_count;
    Material* materials;
    unsigned int materials_count;
} Scene;

typedef struct Accessor {
    void* data;
    ComponentType component_type;
    unsigned int elements_count;
    DataType data_type;
} Accessor;

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define GET_ELEMENT(type, arr, index) ((type) (((arr).data)[index]))
#define STR_LEN(str, len) while ((str)[len] != '\0') { len++; }
#define SET_COLOR(color) printf("\033[%d;1m", color)
#define RESET_COLOR() printf("\033[0m")
#define FALSE 0
#define TRUE 1

#endif //_TYPES_H_