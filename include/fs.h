#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_FILENAME 32
#define MAX_FILES 64
#define MAX_FILE_SIZE 4096

// File entry structure
typedef struct {
    char name[MAX_FILENAME];
    uint8_t *data;
    size_t size;
    bool used;
} file_entry_t;

// Filesystem structure
typedef struct {
    file_entry_t files[MAX_FILES];
    int file_count;
} filesystem_t;

// Filesystem functions
void fs_init(void);
int fs_create_file(const char *name, const uint8_t *data, size_t size);
int fs_read_file(const char *name, uint8_t *buffer, size_t buffer_size);
int fs_delete_file(const char *name);
int fs_list_files(char *buffer, size_t buffer_size);
bool fs_file_exists(const char *name);
size_t fs_get_file_size(const char *name);

// File operations for kernel use
int fs_write_file(const char *name, const uint8_t *data, size_t size);