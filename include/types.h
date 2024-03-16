#ifndef _TYPES_H_
#define _TYPES_H_

typedef enum DecodingError {NO_ERROR, FILE_NOT_FOUND, INVALID_FILE_TYPE, FILE_ERROR, INVALID_MARKER_LENGTH, INVALID_QUANTIZATION_TABLE_NUM, INVALID_HUFFMAN_TABLE_NUM, INVALID_IMAGE_SIZE, EXCEEDED_LENGTH, UNSUPPORTED_JPEG_TYPE, INVALID_DEPTH_COLOR_COMBINATION, INVALID_CHUNK_LENGTH, INVALID_COMPRESSION_METHOD, INVALID_FILTER_METHOD, INVALID_INTERLACE_METHOD, INVALID_IEND_CHUNK_SIZE, DECODING_ERROR} DecodingError; 
typedef enum Colors {RED = 31, GREEN, YELLOW, BLUE, PURPLE, CYAN, WHITE} Colors;
typedef unsigned char bool;

const char* err_codes[] = {"NO_ERROR", "FILE_NOT_FOUND", "INVALID_FILE_TYPE", "FILE_ERROR", "INVALID_MARKER_LENGTH", "INVALID_QUANTIZATION_TABLE_NUM", "INVALID_HUFFMAN_TABLE_NUM", "INVALID_IMAGE_SIZE", "EXCEEDED_LENGTH", "UNSUPPORTED_JPEG_TYPE", "INVALID_DEPTH_COLOR_COMBINATION", "INVALID_CHUNK_LENGTH", "INVALID_COMPRESSION_METHOD", "INVALID_FILTER_METHOD", "INVALID_INTERLACE_METHOD", "INVALID_IEND_CHUNK_SIZE", "DECODING_ERROR"};

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
    DecodingError error;
} BitStream;

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define SET_COLOR(color) printf("\033[%d;1m", color)
#define RESET_COLOR() printf("\033[0m")
#define FALSE 0
#define TRUE 1

#endif //_TYPES_H_