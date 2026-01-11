#include "../include/kernel.h"
#include "../hal/serial.h"
#include "../include/fs.h"
#include <string.h>

// Global filesystem instance
static filesystem_t fs;

void fs_init(void) {
    memset(&fs, 0, sizeof(filesystem_t));
    fs.file_count = 0;

    // Create some default files
    const char *readme_content = "Welcome to Tiny64!\n\nThis is a simple operating system.\n";
    fs_create_file("README.txt", (const uint8_t*)readme_content, strlen(readme_content));

    const char *config_content = "[system]\nversion=1.0\n";
    fs_create_file("config.ini", (const uint8_t*)config_content, strlen(config_content));

    serial_write_string("[FS] Filesystem initialized with default files\n");
}

int fs_create_file(const char *name, const uint8_t *data, size_t size) {
    if (!name || !data || size > MAX_FILE_SIZE) {
        return -1;
    }

    // Check if file already exists
    if (fs_file_exists(name)) {
        return -2; // File already exists
    }

    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs.files[i].used) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        return -3; // No free slots
    }

    // Allocate memory for file data
    uint8_t *file_data = kmalloc(size);
    if (!file_data) {
        return -4; // Memory allocation failed
    }

    // Copy data
    memcpy(file_data, data, size);

    // Create file entry
    strncpy(fs.files[slot].name, name, MAX_FILENAME - 1);
    fs.files[slot].name[MAX_FILENAME - 1] = '\0';
    fs.files[slot].data = file_data;
    fs.files[slot].size = size;
    fs.files[slot].used = true;
    fs.file_count++;

    return 0;
}

int fs_write_file(const char *name, const uint8_t *data, size_t size) {
    // Delete existing file if it exists
    fs_delete_file(name);
    return fs_create_file(name, data, size);
}

int fs_read_file(const char *name, uint8_t *buffer, size_t buffer_size) {
    if (!name || !buffer) {
        return -1;
    }

    // Find file
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].used && strcmp(fs.files[i].name, name) == 0) {
            size_t copy_size = (fs.files[i].size < buffer_size) ? fs.files[i].size : buffer_size;
            memcpy(buffer, fs.files[i].data, copy_size);
            return copy_size;
        }
    }

    return -2; // File not found
}

int fs_delete_file(const char *name) {
    if (!name) {
        return -1;
    }

    // Find and delete file
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].used && strcmp(fs.files[i].name, name) == 0) {
            kfree(fs.files[i].data);
            memset(&fs.files[i], 0, sizeof(file_entry_t));
            fs.file_count--;
            return 0;
        }
    }

    return -2; // File not found
}

int fs_list_files(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return -1;
    }

    buffer[0] = '\0';
    size_t used = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].used) {
            size_t needed = strlen(fs.files[i].name) + 2; // +2 for newline
            if (used + needed >= buffer_size) {
                break;
            }
            strcat(buffer, fs.files[i].name);
            strcat(buffer, "\n");
            used += needed;
        }
    }

    return used;
}

bool fs_file_exists(const char *name) {
    if (!name) {
        return false;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].used && strcmp(fs.files[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

size_t fs_get_file_size(const char *name) {
    if (!name) {
        return 0;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.files[i].used && strcmp(fs.files[i].name, name) == 0) {
            return fs.files[i].size;
        }
    }

    return 0;
}