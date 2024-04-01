#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include "./types.h"

#define GET_US_ELEMENT_LE(arr, ind) (unsigned short int) (((arr)[(ind) + 1] << 8) + (arr)[(ind)]) 
#define GET_UI_ELEMENT_LE(arr, ind) (unsigned int) (((arr)[(ind) + 3] << 24) + ((arr)[(ind) + 2] << 16) + ((arr)[(ind) + 1] << 8) + (arr)[(ind)])

/* -------------------------------------------------------------------------- */

float* multiply_mat4(float* a, float* b);
int s_atoi(char* value);
bool str_to_bool(char* str, char* true_str);
void strip(char** str);
Array init_arr();
void append_element(Array* arr, void* element);
void deallocate_arr(Array arr);

/* -------------------------------------------------------------------------- */

const float id_mat[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};

float* multiply_mat4(float* a, float* b) {
    float* new_mat = (float*) calloc(16, sizeof(float));
    for (unsigned char r = 0; r < 4; ++r) {
        for (unsigned char c = 0; c < 4; ++c) {
            for (unsigned char s = 0; s < 4; ++s) {
                new_mat[r * 4 + c] += a[r * 4 + s] * b[s * 4 + c];
            }
        }
    }
    return new_mat;
}

int s_atoi(char* value) {
    if (value == NULL) return 0;
    return atoi(value);
}

bool str_to_bool(char* str, char* true_str) {
    if (!strcmp(str, true_str)) return TRUE;
    return FALSE;
}

void strip(char** str) {
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
    debug_print(YELLOW, "deallocating array...\n");
    free(arr.data);
    return;
}

#endif //_UTILS_H_