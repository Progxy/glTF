#ifdef _GLTF_LIB_

#include "./bitstream.h"
#include "./debug_print.h"
#include "./file_io.h"
#include "./types.h"
#include "./utils.h"
#include "./gltf_loader.h"

static void append_obj(Object* parent_obj, Object obj) {
    parent_obj -> children = (Object*) realloc(parent_obj -> children, sizeof(Object) * (parent_obj -> children_count + 1));
    (parent_obj -> children)[parent_obj -> children_count] = obj;
    (parent_obj -> children)[parent_obj -> children_count].parent = parent_obj;
    (parent_obj -> children_count)++;
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

static void* get_array(Object* arr_obj, bool use_float) {
    if (arr_obj == NULL) {
        return NULL;
    }

    void* arr = calloc(arr_obj -> children_count, sizeof(unsigned int));
    for (unsigned int i = 0; i < arr_obj -> children_count; ++i) {
        if (use_float) ((float*) arr)[i] = (float) atof((char*) ((arr_obj -> children)[i].value));
        else ((unsigned int*) arr)[i] = atoi((char*) ((arr_obj -> children)[i].value));
    }
    
    return arr;
}

static void* get_value(Object* obj) {
    if (obj == NULL) return NULL;
    return obj -> value;
}

static Node create_node(Object* nodes_obj, unsigned int node_index) {
    Node node = {0};
    Object* node_obj = nodes_obj -> children + node_index;

    Object* translation_obj = get_object_by_id("translation", node_obj, FALSE);
    if (translation_obj != NULL) {
        for (unsigned char i = 0; i < 3; ++i) {
            node.translation_vec[i] = atof((char*) ((translation_obj -> children + i) -> value));
        }
    } else {
        for (unsigned char i = 0; i < 3; ++i) {
            node.translation_vec[i] = 0.0f;
        }
    }    
    
    Object* rotation_obj = get_object_by_id("rotation", node_obj, FALSE);
    if (rotation_obj != NULL) {
        node.rotation_quat[0] = atof((char*) ((rotation_obj -> children + 3) -> value));
        for (unsigned char i = 0; i < 3; ++i) {
            node.rotation_quat[i + 1] = atof((char*) ((rotation_obj -> children + i) -> value));
        }
    } else {
        node.rotation_quat[3] = 1.0f;
        for (unsigned char i = 0; i < 3; ++i) {
            node.rotation_quat[i + 1] = 0.0f;
        }
    }

    Object* scale_obj = get_object_by_id("scale", node_obj, FALSE);
    if (scale_obj != NULL) {
        for (unsigned char i = 0; i < 3; ++i) {
            node.scale_vec[i] = atof((char*) ((scale_obj -> children + i) -> value));
        }
    } else {
        for (unsigned char i = 0; i < 3; ++i) {
            node.scale_vec[i] = 1.0f;
        }
    }        

    Object* matrix_obj = get_object_by_id("matrix", node_obj, FALSE);
    if (matrix_obj != NULL) {
        for (unsigned char r = 0; r < 4; ++r) {
            for (unsigned char c = 0; c < 4; ++c) {
                unsigned char index = r * 4 + c;
                node.transformation_matrix[index] = atof((char*) ((matrix_obj -> children + index) -> value));
            }
        }
    } else {
        for (unsigned char i = 0; i < 4; ++i) {
            node.transformation_matrix[i * 4 + i] = 1.0f;
        }
    }

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
        if (meshes -> obj_type == ARRAY) {
            unsigned int meshes_count = meshes -> children_count;
            for (unsigned int i = 0; i < meshes_count; ++i) {
                unsigned int* mesh_index = (unsigned int*) calloc(1, sizeof(unsigned int));
                *mesh_index = atoi((char*) ((meshes -> children)[i].value));
                append_element(&(node.meshes_indices), mesh_index);
            }
        }
        unsigned int* mesh_index = (unsigned int*) calloc(1, sizeof(unsigned int));
        *mesh_index = atoi((char*) (meshes -> value));
        append_element(&(node.meshes_indices), mesh_index);
    } else {
        node.meshes_indices = (Array) { .count = 0, .data = NULL };
    }

    return node;
}

static Array decode_buffer_views(Object main_obj, char* path) {
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
        read_model_file(&buffer_data);
        
        BitStream* bit_stream = allocate_bit_stream(buffer_data.data, byte_length, TRUE);
        append_element(&buffers, (void*) bit_stream);
        deallocate_file(&buffer_data, TRUE);
    }

    // Store buffer views
    Object* buffer_views_obj = get_object_by_id("bufferViews", &main_obj, TRUE);
    for (unsigned int i = 0; i < buffer_views_obj -> children_count; ++i) {
        unsigned int buffer_index = atoi((char*)(get_object_by_id("buffer", buffer_views_obj -> children + i, TRUE) -> value));
        unsigned int byte_length = atoi((char*)(get_object_by_id("byteLength", buffer_views_obj -> children + i, TRUE) -> value));
        unsigned int byte_offset = s_atoi((char*)(get_value(get_object_by_id("byteOffset", buffer_views_obj -> children + i, TRUE))));

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

static DataType get_data_type(char* data_type_str) {
    if (!strcmp("SCALAR", data_type_str)) return SCALAR;
    else if (!strcmp("VEC2", data_type_str)) return VEC2;
    else if (!strcmp("VEC3", data_type_str)) return VEC3;
    else if (!strcmp("VEC4", data_type_str)) return VEC4;
    else if (!strcmp("MAT2", data_type_str)) return MAT2;
    else if (!strcmp("MAT3", data_type_str)) return MAT3;
    else if (!strcmp("MAT4", data_type_str)) return MAT4;
    else return SCALAR;
}

static void decode_accessors(Object main_obj, char* path, Array* accessors) {
    Array buffer_views = decode_buffer_views(main_obj, path);

    Object* accessors_obj = get_object_by_id("accessors", &main_obj, TRUE);
    for (unsigned int i = 0; i < accessors_obj -> children_count; ++i) {
        unsigned int buffer_view_index = atoi((char*) (get_object_by_id("bufferView", accessors_obj -> children + i, TRUE) -> value));
        ComponentType component_type = atoi((char*) (get_object_by_id("componentType", accessors_obj -> children + i, TRUE) -> value)) % 5120;
        unsigned int total_elements = atoi((char*) (get_object_by_id("count", accessors_obj -> children + i, TRUE) -> value));
        unsigned int byte_offset = s_atoi((char*)(get_value(get_object_by_id("byteOffset", accessors_obj -> children + i, TRUE))));
        DataType data_type = get_data_type((char*) (get_object_by_id("type", accessors_obj -> children + i, TRUE) -> value));
        BitStream* buffer_view_stream = GET_ELEMENT(BitStream*, buffer_views, buffer_view_index);
        buffer_view_stream -> byte = byte_offset;

        Accessor* accessor = (Accessor*) calloc(1, sizeof(Accessor));
        *accessor = (Accessor) { .component_type = component_type, .elements_count = total_elements, .data_type = data_type };
        accessor -> data = get_next_n_byte(buffer_view_stream, total_elements * elements_count[data_type], byte_lengths[component_type]);
        append_element(accessors, accessor);
    }

    // Deallocate buffer_views
    for (unsigned int i = 0; i < buffer_views.count; ++i) {
        deallocate_bit_stream(GET_ELEMENT(BitStream*, buffer_views, i));
    }
    deallocate_arr(buffer_views);
    
    return;
}

static void extract_elements(Accessor* obj_accessor, ArrayExtended* arr_ext) {
    arr_ext -> arr = init_arr();
    for (unsigned int s = 0; s < obj_accessor -> elements_count; ++s) {
        unsigned char element_size = elements_count[obj_accessor -> data_type];
        unsigned char byte_size = byte_lengths[obj_accessor -> component_type];
        void* element = calloc(element_size, byte_size);
        unsigned char* data = (unsigned char*) (obj_accessor -> data); 
        unsigned int offset = s * element_size * byte_size;

        for (unsigned char t = 0; t < element_size; ++t) {
            unsigned int current_index = (t * byte_size) + offset;
            if (obj_accessor -> component_type == BYTE) ((char*) element)[t] = data[current_index];
            else if (obj_accessor -> component_type == UNSIGNED_BYTE) ((unsigned char*) element)[t] = data[current_index];
            else if (obj_accessor -> component_type == SHORT) ((short int*) element)[t] = (short int) GET_US_ELEMENT_LE(data, current_index);
            else if (obj_accessor -> component_type == UNSIGNED_SHORT) ((unsigned short int*) element)[t] = GET_US_ELEMENT_LE(data, current_index);
            else if (obj_accessor -> component_type == UNSIGNED_INT) ((unsigned int*) element)[t] = GET_UI_ELEMENT_LE(data, current_index);
            else if (obj_accessor -> component_type == FLOAT) {
                unsigned int value = GET_UI_ELEMENT_LE(data, current_index);
                ((float*) element)[t] = *((float*) (&value));
            }
        }
        
        append_element(&(arr_ext -> arr), element);
    }

    arr_ext -> component_type = obj_accessor -> component_type; 
    arr_ext -> data_type = obj_accessor -> data_type;
    
    return;
}

static Face* create_faces(Accessor* indices_accessor, Topology topology, unsigned int* faces_count) {
    Face* faces = (Face*) calloc(1, sizeof(Face)); 
    *faces_count = 0;
    unsigned char* data = (unsigned char*) (indices_accessor -> data);
    unsigned int total_faces = indices_accessor -> elements_count;
    if (topology == TRIANGLE_STRIP || topology == TRIANGLE_FAN) total_faces -= 2;
    else if (topology == LINE_STRIP) total_faces -= 1;
    else if (topology == LINES) total_faces /= 2;
    else if (topology == TRIANGLES) total_faces /= 3;

    for (unsigned int i = 0; i < total_faces; ++i, ++(*faces_count)) {
        faces = (Face*) realloc(faces, sizeof(Face) * (*faces_count + 1));
        faces[i].topology = topology;
        faces[i].indices = (unsigned int*) calloc(topology_size[topology], sizeof(unsigned int));
        if (topology == LINE_STRIP) {
            faces[i].indices[0] = GET_UI_ELEMENT_LE(data, i * sizeof(unsigned int));
            faces[i].indices[1] = GET_UI_ELEMENT_LE(data, (i + 1) * sizeof(unsigned int));
        } else if (topology == LINE_LOOP) {
            faces[i].indices[0] = GET_UI_ELEMENT_LE(data, i * sizeof(unsigned int));
            faces[i].indices[1] = GET_UI_ELEMENT_LE(data, ((i + 1) % indices_accessor -> elements_count) * sizeof(unsigned int));
        } else if (topology == LINES) {
            faces[i].indices[0] = GET_UI_ELEMENT_LE(data, 2 * i * sizeof(unsigned int));
            faces[i].indices[1] = GET_UI_ELEMENT_LE(data, ((2 * i) + 1) * sizeof(unsigned int));
        } else if (topology == TRIANGLES) {
            faces[i].indices[0] = GET_UI_ELEMENT_LE(data, 3 * i * sizeof(unsigned int));
            faces[i].indices[1] = GET_UI_ELEMENT_LE(data, ((3 * i) + 1) * sizeof(unsigned int));
            faces[i].indices[2] = GET_UI_ELEMENT_LE(data, ((3 * i) + 2) * sizeof(unsigned int));
        } else if (topology == TRIANGLE_STRIP) {
            faces[i].indices[0] = GET_UI_ELEMENT_LE(data, i * sizeof(unsigned int));
            faces[i].indices[1] = GET_UI_ELEMENT_LE(data, (i + (1 + i % 2)) * sizeof(unsigned int));
            faces[i].indices[2] = GET_UI_ELEMENT_LE(data, (i + (2 - i % 2)) * sizeof(unsigned int));
        } else if (topology == TRIANGLE_FAN) {
            faces[i].indices[0] = GET_UI_ELEMENT_LE(data, (i + 1) * sizeof(unsigned int));
            faces[i].indices[1] = GET_UI_ELEMENT_LE(data, (i + 2) * sizeof(unsigned int));
            faces[i].indices[2] = GET_UI_ELEMENT_LE(data, 0);
        } else if (topology == POINTS) faces[i].indices[0] = GET_UI_ELEMENT_LE(data, i * sizeof(unsigned int));
    }

    return faces;
}

static Mesh* decode_mesh(Array accessors, Object main_obj, unsigned int* meshes_count) {
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
            unsigned int tangent_index = atoi((char*) (get_object_by_id("attributes/TANGENT", primitives -> children + j, TRUE) -> value));
            unsigned int tex_coords_index = atoi((char*) (get_object_by_id("attributes/TEXCOORD_0", primitives -> children + j, TRUE) -> value));

            Accessor* vertex_accessor = GET_ELEMENT(Accessor*, accessors, vertices_index);
            extract_elements(vertex_accessor, &(meshes[i].vertices));            
            Accessor* normal_accessor = GET_ELEMENT(Accessor*, accessors, normal_index);
            extract_elements(normal_accessor, &(meshes[i].normals));            
            Accessor* tangent_accessor = GET_ELEMENT(Accessor*, accessors, tangent_index);
            extract_elements(tangent_accessor, &(meshes[i].tangents));            
            Accessor* tex_coords_accessor = GET_ELEMENT(Accessor*, accessors, tex_coords_index);
            extract_elements(tex_coords_accessor, &(meshes[i].texture_coords));
            Accessor* indices_accessor = GET_ELEMENT(Accessor*, accessors, indices_index);
            meshes[i].faces = create_faces(indices_accessor, topology, &(meshes[i].faces_count));
            meshes[i].material_index = material_index;
        }
    }

    return meshes;
}

static Texture* collect_textures(Object main_obj, unsigned int* texture_count, char* path) {
    Texture* textures = (Texture*) calloc(1, sizeof(Texture));
    Object* textures_obj = get_object_by_id("textures", &main_obj, TRUE);
    Object* sampler_obj = get_object_by_id("samplers", &main_obj, TRUE);
    Object* images_obj = get_object_by_id("images", &main_obj, TRUE);

    for (unsigned int i = 0; i < textures_obj ->children_count; ++i, ++(*texture_count)) {
        textures = (Texture*) realloc(textures, sizeof(Texture) * (*texture_count + 1));
        unsigned int sampler_id = atoi((char*) (get_object_by_id("sampler", textures_obj -> children + i, TRUE) -> value));
        unsigned int source_id = atoi((char*) (get_object_by_id("source", textures_obj -> children + i, TRUE) -> value));
        textures[i].mag_filter = atoi((char*) (get_object_by_id("magFilter", sampler_obj -> children + sampler_id, TRUE) -> value));
        textures[i].min_filter = atoi((char*) (get_object_by_id("minFilter", sampler_obj -> children + sampler_id, TRUE) -> value));
        textures[i].wrap_s = atoi((char*) (get_object_by_id("wrapS", sampler_obj -> children + sampler_id, TRUE) -> value));
        textures[i].wrap_t = atoi((char*) (get_object_by_id("wrapT", sampler_obj -> children + sampler_id, TRUE) -> value));
        textures[i].texture_path = (char*) calloc(350, sizeof(char));
        int path_len = snprintf(textures[i].texture_path, 350, "%s%s", path, (char*) (get_object_by_id("uri", images_obj -> children + source_id, TRUE) -> value));
        textures[i].texture_path = (char*) realloc(textures[i].texture_path, sizeof(char) * path_len);
        textures[i].tex_coord = -1;
    }
    
    return textures;
}

static Material* decode_materials(Object main_obj, unsigned int* materials_count, char* path) {
    Material* materials = (Material*) calloc(1, sizeof(Material));

    unsigned int texture_count = 0;
    Texture* textures = collect_textures(main_obj, &texture_count, path);

    Object* materials_obj = get_object_by_id("materials", &main_obj, TRUE);
    for (unsigned int i = 0; i < materials_obj -> children_count; ++i, ++(*materials_count)) {
        materials = (Material*) realloc(materials, sizeof(Material) * (*materials_count + 1));

        Object* pbr_metallic_roughness_obj = get_object_by_id("pbrMetallicRoughness", materials_obj -> children + i, FALSE);
        if (pbr_metallic_roughness_obj != NULL) {
            unsigned int base_color_texture_index = atoi((char*) (get_object_by_id("baseColorTexture/index", pbr_metallic_roughness_obj, TRUE) -> value));
            materials[i].pbr_metallic_roughness.base_color_texture = textures[base_color_texture_index];

            materials[i].pbr_metallic_roughness.base_color_factor = (float*) get_array(get_object_by_id("baseColorFactor", pbr_metallic_roughness_obj, FALSE), TRUE);
            materials[i].pbr_metallic_roughness.base_color_texture.tex_coord = s_atoi((char*) get_value(get_object_by_id("baseColorTexture/texCoord", pbr_metallic_roughness_obj, FALSE))); 
            materials[i].pbr_metallic_roughness.metallic_factor = s_atoi((char*) get_value(get_object_by_id("metallicFactor", pbr_metallic_roughness_obj, FALSE))); 
            materials[i].pbr_metallic_roughness.roughness_factor = s_atoi((char*) get_value(get_object_by_id("roughnessFactor", pbr_metallic_roughness_obj, FALSE))); 
            materials[i].pbr_metallic_roughness.roughness_factor = s_atoi((char*) get_value(get_object_by_id("roughnessFactor", pbr_metallic_roughness_obj, FALSE))); 

            Object* metallic_roughness_texture_index_obj = get_object_by_id("metallicRoughnessTexture/index", pbr_metallic_roughness_obj, FALSE);
            if (metallic_roughness_texture_index_obj != NULL) {
                materials[i].pbr_metallic_roughness.metallic_roughness_texture = textures[atoi((char*) (metallic_roughness_texture_index_obj -> value))];
                materials[i].pbr_metallic_roughness.metallic_roughness_texture.tex_coord = s_atoi((char*) get_value(get_object_by_id("metallicRoughnessTexture/texCoord", pbr_metallic_roughness_obj, FALSE))); 
            } 
        }  

        Object* normal_texture_index_obj = get_object_by_id("normalTexture/index", materials_obj -> children + i, FALSE);
        if (normal_texture_index_obj != NULL) {
            materials[i].normal_texture.texture = textures[atoi((char*) (normal_texture_index_obj -> value))];
            materials[i].normal_texture.texture.tex_coord = s_atoi((char*) get_value(get_object_by_id("normalTexture/texCoord", materials_obj -> children + i, FALSE))); 
            materials[i].normal_texture.scale = s_atoi((char*) get_value(get_object_by_id("normalTexture/scale", materials_obj -> children + i, FALSE))); 
        }       
        
        Object* occlusion_texture_index_obj = get_object_by_id("occlusionTexture/index", materials_obj -> children + i, FALSE);
        if (occlusion_texture_index_obj != NULL) {
            materials[i].occlusion_texture.texture = textures[atoi((char*) (occlusion_texture_index_obj -> value))];
            materials[i].occlusion_texture.texture.tex_coord = s_atoi((char*) get_value(get_object_by_id("occlusionTexture/texCoord", materials_obj -> children + i, FALSE))); 
            materials[i].occlusion_texture.strength = s_atoi((char*) get_value(get_object_by_id("occlusionTexture/strength", materials_obj -> children + i, FALSE))); 
        }       

        Object* emissive_texture_index_obj = get_object_by_id("emissiveTexture/index", materials_obj -> children + i, FALSE);
        if (emissive_texture_index_obj != NULL) {
            materials[i].emissive_texture = textures[atoi((char*) (emissive_texture_index_obj -> value))];
            materials[i].emissive_texture.tex_coord = s_atoi((char*) get_value(get_object_by_id("emissiveTexture/texCoord", materials_obj -> children + i, FALSE))); 
        }       

        materials[i].emissive_factor = (float*) get_array(get_object_by_id("emissiveFactor", pbr_metallic_roughness_obj, FALSE), TRUE);
        Object* alpha_mode_obj = get_object_by_id("alphaMode", materials_obj -> children + i, FALSE);
        materials[i].alpha_mode = (alpha_mode_obj != NULL) ? (char*) (alpha_mode_obj -> value) : NULL;
        materials[i].alpha_cutoff = s_atoi((char*) get_value(get_object_by_id("alphaCutoff", materials_obj -> children + i, FALSE))); 
        Object* double_sided_obj = get_object_by_id("doubleSided", materials_obj -> children + i, FALSE);
        materials[i].double_sided = (double_sided_obj != NULL) ? str_to_bool((char*) (double_sided_obj ->  value), "true") : FALSE;
    }

    // Deallocate textures
    free(textures);

    return materials;
}

static Scene decode_scene(Object main_obj, char* path) {
    Scene scene = {0};

    Array accessors = init_arr();
    decode_accessors(main_obj, path, &accessors);

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

Scene decode_gltf(char* path) {
    char* file_path = (char*) calloc(175, sizeof(char));
    int len = snprintf(file_path, 175, "%sscene.gltf", path);
    file_path = (char*) realloc(file_path, sizeof(char) * len);

    File file_data = (File) {.file_path = file_path};
    read_model_file(&file_data);

    BitStream* bit_stream = allocate_bit_stream(file_data.data, file_data.size, FALSE);
    deallocate_file(&file_data, FALSE);

    Scene scene = {0};

    if (get_next_bytes_us(bit_stream) != (unsigned short int)(('{' << 8) | '\n')) {
        error_print("invalid gltf file\n");
        deallocate_bit_stream(bit_stream);
        return scene; 
    }

    Object default_object = (Object) { .children = calloc(1, sizeof(Object)), .children_count = 0, .parent = NULL, .value = NULL, .identifier = "main", .obj_type = DICTIONARY };
    read_dictionary(bit_stream, &default_object);
    deallocate_bit_stream(bit_stream);

    scene = decode_scene(default_object, path);

    return scene;
}

#endif //_GLTF_LIB_
