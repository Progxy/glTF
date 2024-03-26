#ifndef _DEBUG_PRINT_H_
#define _DEBUG_PRINT_H_

#include <stdio.h>
#include <stdarg.h>
#include "./types.h"

void error_print(const char* format, ...) {
    SET_COLOR(RED);
    printf("ERROR: ");
    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
    RESET_COLOR();
    return;
}

void warning_print(const char* format, ...) {
    SET_COLOR(YELLOW);
    printf("WARNING: ");
    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
    RESET_COLOR();
    return;
}

#ifdef _DEBUG_MODE_ 

void debug_print(Colors color, const char* format, ...) {
    SET_COLOR(color);
    if (format[1] == '\0') {
        printf("%s", format);
        return;
    }
    
    printf("DEBUG_INFO: ");
    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
    RESET_COLOR();
    return;
}

#else

#define NOT_USED(var) (void) var

void debug_print(Colors color, const char* format, ...) {
    NOT_USED(color);
    NOT_USED(format);
    return;
}

#endif //_DEBUG_MODE_

#endif //_DEBUG_PRINT_H_