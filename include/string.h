#pragma once
#include <stdint.h>
#include <stddef.h>

// Basic string and memory functions for kernel use

// Memory functions
void* memset(void* dest, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);

// String functions
size_t strlen(const char* str);
int strcmp(const char* str1, const char* str2);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strchr(const char* str, int c);