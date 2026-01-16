#include "../include/kernel.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/ttf.h"
#include "../hal/serial.h"

// Helper functions for big-endian reading
static uint16_t read_uint16_be(const uint8_t *data) {
    return (data[0] << 8) | data[1];
}

static uint32_t read_uint32_be(const uint8_t *data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static int32_t read_int32_be(const uint8_t *data) {
    return (int32_t)read_uint32_be(data);
}

static int64_t read_int64_be(const uint8_t *data) {
    return ((int64_t)read_uint32_be(data) << 32) | read_uint32_be(data + 4);
}

static int16_t read_int16_be(const uint8_t *data) {
    return (int16_t)read_uint16_be(data);
}

// Find table by tag
static ttf_table_directory_t* ttf_find_table(ttf_font_t *font, uint32_t tag) {
    for (uint16_t i = 0; i < font->offset_table.num_tables; i++) {
        if (font->table_directory[i].tag == tag) {
            return &font->table_directory[i];
        }
    }
    return NULL;
}

int ttf_load_font(const char *filename, ttf_font_t *font) {
    if (!filename || !font) return -1;

    // Initialize font structure
    memset(font, 0, sizeof(ttf_font_t));

    // Load font file from filesystem
    font->font_size = fs_get_file_size(filename);
    if (font->font_size == 0) {
        serial_write_string("[TTF] Font file not found or empty: ");
        serial_write_string(filename);
        serial_write_string("\n");
        return -1;
    }

    font->font_data = kmalloc(font->font_size);
    if (!font->font_data) {
        serial_write_string("[TTF] Failed to allocate memory for font\n");
        return -1;
    }

    if (fs_read_file(filename, font->font_data, font->font_size) != (int)font->font_size) {
        serial_write_string("[TTF] Failed to read font file\n");
        kfree(font->font_data);
        return -1;
    }

    serial_write_string("[TTF] Loaded font file, size: ");
    // Simple size logging
    char size_str[16] = "00000000";
    int temp_size = font->font_size;
    for (int i = 7; i >= 0 && temp_size > 0; i--) {
        size_str[i] = '0' + (temp_size % 10);
        temp_size /= 10;
    }
    serial_write_string(size_str);
    serial_write_string(" bytes\n");

    // Parse the font
    return ttf_parse_font(font);
}

int ttf_parse_font(ttf_font_t *font) {
    // Parse offset table with safety checks
    if (font->font_size < sizeof(ttf_offset_table_t)) {
        serial_write_string("[TTF] Font file too small for offset table\n");
        goto error;
    }

    // Additional validation
    if (font->font_size > 10 * 1024 * 1024) { // 10MB limit
        serial_write_string("[TTF] Font file too large\n");
        goto error;
    }

    const uint8_t *data = font->font_data;
    font->offset_table.scaler_type = read_uint32_be(data);
    font->offset_table.num_tables = read_uint16_be(data + 4);
    font->offset_table.search_range = read_uint16_be(data + 6);
    font->offset_table.entry_selector = read_uint16_be(data + 8);
    font->offset_table.range_shift = read_uint16_be(data + 10);

    serial_write_string("[TTF] Offset table: ");
    char num_str[8] = "00000";
    int temp_num = font->offset_table.num_tables;
    for (int i = 4; i >= 0 && temp_num > 0; i--) {
        num_str[i] = '0' + (temp_num % 10);
        temp_num /= 10;
    }
    serial_write_string(num_str);
    serial_write_string(" tables\n");

    // Allocate table directory
    size_t table_dir_size = font->offset_table.num_tables * sizeof(ttf_table_directory_t);
    font->table_directory = kmalloc(table_dir_size);
    if (!font->table_directory) {
        serial_write_string("[TTF] Failed to allocate table directory\n");
        goto error;
    }

    // Parse table directory with bounds checking
    size_t table_dir_data_size = font->offset_table.num_tables * 16;
    if (sizeof(ttf_offset_table_t) + table_dir_data_size > font->font_size) {
        serial_write_string("[TTF] Table directory out of bounds\n");
        goto error;
    }

    const uint8_t *table_data = data + sizeof(ttf_offset_table_t);
    for (uint16_t i = 0; i < font->offset_table.num_tables; i++) {
        if (i * 16 + 16 > table_dir_data_size) {
            serial_write_string("[TTF] Table directory entry out of bounds\n");
            goto error;
        }
        font->table_directory[i].tag = read_uint32_be(table_data + i * 16);
        font->table_directory[i].check_sum = read_uint32_be(table_data + i * 16 + 4);
        font->table_directory[i].offset = read_uint32_be(table_data + i * 16 + 8);
        font->table_directory[i].length = read_uint32_be(table_data + i * 16 + 12);

        // Validate table bounds
        if (font->table_directory[i].offset + font->table_directory[i].length > font->font_size) {
            serial_write_string("[TTF] Table extends beyond file bounds\n");
            goto error;
        }
    }

    // Find required tables
    ttf_table_directory_t *head_table_dir = ttf_find_table(font, 0x68656164); // 'head'
    ttf_table_directory_t *cmap_table_dir = ttf_find_table(font, 0x636d6170); // 'cmap'

    if (!head_table_dir) {
        serial_write_string("[TTF] Required table 'head' not found\n");
        goto error;
    }

    if (head_table_dir->offset + sizeof(ttf_head_table_t) > font->font_size) {
        serial_write_string("[TTF] Head table out of bounds\n");
        goto error;
    }

    // Parse head table
    const uint8_t *head_data = font->font_data + head_table_dir->offset;
    font->head_table = kmalloc(sizeof(ttf_head_table_t));
    if (!font->head_table) {
        serial_write_string("[TTF] Failed to allocate head table\n");
        goto error;
    }

    font->head_table->major_version = read_uint16_be(head_data);
    font->head_table->minor_version = read_uint16_be(head_data + 2);
    font->head_table->font_revision = read_int32_be(head_data + 4);
    font->head_table->check_sum_adjustment = read_uint32_be(head_data + 8);
    font->head_table->magic_number = read_uint32_be(head_data + 12);
    font->head_table->flags = read_uint16_be(head_data + 16);
    font->head_table->units_per_em = read_uint16_be(head_data + 18);
    font->head_table->created = read_int64_be(head_data + 20);
    font->head_table->modified = read_int64_be(head_data + 28);
    font->head_table->x_min = read_int16_be(head_data + 36);
    font->head_table->y_min = read_int16_be(head_data + 38);
    font->head_table->x_max = read_int16_be(head_data + 40);
    font->head_table->y_max = read_int16_be(head_data + 42);
    font->head_table->mac_style = read_uint16_be(head_data + 44);
    font->head_table->lowest_rec_ppem = read_uint16_be(head_data + 46);
    font->head_table->font_direction_hint = read_int16_be(head_data + 48);
    font->head_table->index_to_loc_format = read_int16_be(head_data + 50);
    font->head_table->glyph_data_format = read_int16_be(head_data + 52);

    font->units_per_em = font->head_table->units_per_em;

    // Parse CMAP table for character to glyph mapping (if present)
    if (cmap_table_dir) {
        const uint8_t *cmap_data = font->font_data + cmap_table_dir->offset;
        uint16_t num_encodings = read_uint16_be(cmap_data + 2); // cmap header: version(2), num_tables(2)

        // Look for Unicode BMP (Basic Multilingual Plane) encoding
        for (uint16_t i = 0; i < num_encodings; i++) {
            const uint8_t *encoding_entry = cmap_data + 4 + i * 8;
            uint16_t platform_id = read_uint16_be(encoding_entry);
            uint16_t encoding_id = read_uint16_be(encoding_entry + 2);
            uint32_t offset = read_uint32_be(encoding_entry + 4);

            // Look for Unicode encoding (platform 0 or 3)
            if ((platform_id == 0 || platform_id == 3) &&
                (encoding_id == 1 || encoding_id == 3 || encoding_id == 4)) {

                const uint8_t *subtable = cmap_data + offset;
                uint16_t format = read_uint16_be(subtable);
                if (format == 4) { // Format 4 is most common for BMP
                    uint16_t length = read_uint16_be(subtable + 2);
                    uint16_t seg_count_x2 = read_uint16_be(subtable + 6);
                    uint16_t seg_count = seg_count_x2 / 2;

                    // Allocate cmap format 4 structure and arrays
                    font->cmap_format4 = kmalloc(sizeof(ttf_cmap_format4_t));
                    if (!font->cmap_format4) goto error;

                    font->cmap_format4->format = format;
                    font->cmap_format4->length = length;
                    font->cmap_format4->language = read_uint16_be(subtable + 4);
                    font->cmap_format4->seg_count_x2 = seg_count_x2;
                    font->cmap_format4->search_range = read_uint16_be(subtable + 8);
                    font->cmap_format4->entry_selector = read_uint16_be(subtable + 10);
                    font->cmap_format4->range_shift = read_uint16_be(subtable + 12);

                    size_t array_size = seg_count * sizeof(uint16_t);
                    font->cmap_format4->end_code = kmalloc(array_size);
                    font->cmap_format4->start_code = kmalloc(array_size);
                    font->cmap_format4->id_delta = kmalloc(seg_count * sizeof(int16_t));
                    font->cmap_format4->id_range_offset = kmalloc(array_size);
                    if (!font->cmap_format4->end_code || !font->cmap_format4->start_code ||
                        !font->cmap_format4->id_delta || !font->cmap_format4->id_range_offset) {
                        goto error;
                    }

                    uint16_t data_offset = 14;
                    for (uint16_t j = 0; j < seg_count; j++) {
                        font->cmap_format4->end_code[j] = read_uint16_be(subtable + data_offset);
                        data_offset += 2;
                    }
                    data_offset += 2; // Skip reserved pad
                    for (uint16_t j = 0; j < seg_count; j++) {
                        font->cmap_format4->start_code[j] = read_uint16_be(subtable + data_offset);
                        data_offset += 2;
                    }
                    for (uint16_t j = 0; j < seg_count; j++) {
                        font->cmap_format4->id_delta[j] = read_int16_be(subtable + data_offset);
                        data_offset += 2;
                    }
                    for (uint16_t j = 0; j < seg_count; j++) {
                        font->cmap_format4->id_range_offset[j] = read_uint16_be(subtable + data_offset);
                        data_offset += 2;
                    }

                    uint16_t glyph_array_size = (length - data_offset) / 2;
                    font->cmap_format4->glyph_id_array = kmalloc(glyph_array_size * sizeof(uint16_t));
                    if (!font->cmap_format4->glyph_id_array) goto error;
                    for (uint16_t j = 0; j < glyph_array_size; j++) {
                        font->cmap_format4->glyph_id_array[j] = read_uint16_be(subtable + data_offset);
                        data_offset += 2;
                    }

                    break; // Use first suitable cmap subtable
                }
            }
        }
    }

    serial_write_string("[TTF] Font loaded successfully - units per em: ");
    char upem_str[8] = "00000";
    int temp_upem = font->units_per_em;
    for (int i = 4; i >= 0 && temp_upem > 0; i--) {
        upem_str[i] = '0' + (temp_upem % 10);
        temp_upem /= 10;
    }
    serial_write_string(upem_str);
    serial_write_string("\n");

    // Initialize glyph cache
    memset(font->glyph_cache, 0, sizeof(font->glyph_cache));

    return 0;

error:
    if (font->head_table) kfree(font->head_table);
    if (font->table_directory) kfree(font->table_directory);
    if (font->cmap_format4) {
        if (font->cmap_format4->end_code) kfree(font->cmap_format4->end_code);
        if (font->cmap_format4->start_code) kfree(font->cmap_format4->start_code);
        if (font->cmap_format4->id_delta) kfree(font->cmap_format4->id_delta);
        if (font->cmap_format4->id_range_offset) kfree(font->cmap_format4->id_range_offset);
        if (font->cmap_format4->glyph_id_array) kfree(font->cmap_format4->glyph_id_array);
        kfree(font->cmap_format4);
    }
    if (font->font_data) kfree(font->font_data);
    return -1;
}

int ttf_load_font_data(const uint8_t *data, size_t size, ttf_font_t *font) {
    if (!data || size == 0 || !font) return -1;

    // Initialize font structure
    memset(font, 0, sizeof(ttf_font_t));

    // Use provided data directly (copy it)
    font->font_size = size;
    font->font_data = kmalloc(size);
    if (!font->font_data) {
        serial_write_string("[TTF] Failed to allocate memory for font data\n");
        return -1;
    }

    memcpy(font->font_data, data, size);

    serial_write_string("[TTF] Loaded font data, size: ");
    // Simple size logging
    char size_str[16] = "00000000";
    int temp_size = font->font_size;
    for (int i = 7; i >= 0 && temp_size > 0; i--) {
        size_str[i] = '0' + (temp_size % 10);
        temp_size /= 10;
    }
    serial_write_string(size_str);
    serial_write_string(" bytes\n");

    // Parse the font
    return ttf_parse_font(font);
}

void ttf_free_font(ttf_font_t *font) {
    if (!font) return;

    if (font->cmap_format4) {
        if (font->cmap_format4->end_code) kfree(font->cmap_format4->end_code);
        if (font->cmap_format4->start_code) kfree(font->cmap_format4->start_code);
        if (font->cmap_format4->id_delta) kfree(font->cmap_format4->id_delta);
        if (font->cmap_format4->id_range_offset) kfree(font->cmap_format4->id_range_offset);
        if (font->cmap_format4->glyph_id_array) kfree(font->cmap_format4->glyph_id_array);
        kfree(font->cmap_format4);
    }

    if (font->head_table) kfree(font->head_table);
    if (font->table_directory) kfree(font->table_directory);
    if (font->font_data) kfree(font->font_data);

    memset(font, 0, sizeof(ttf_font_t));
}

// Placeholder implementations - will be implemented next
int ttf_get_glyph_index(ttf_font_t *font, uint32_t codepoint) {
    if (!font || !font->cmap_format4 || codepoint > 0xFFFF) {
        return 0; // Missing glyph
    }

    ttf_cmap_format4_t *cmap = font->cmap_format4;
    uint16_t seg_count = cmap->seg_count_x2 / 2;

    // Binary search for the segment containing this codepoint
    uint16_t start = 0;
    uint16_t end = seg_count - 1;

    while (start <= end) {
        uint16_t mid = (start + end) / 2;
        if (cmap->end_code[mid] >= codepoint) {
            end = mid - 1;
        } else {
            start = mid + 1;
        }
    }

    // Now start contains the segment index
    if (start >= seg_count) {
        return 0; // Codepoint not found
    }

    uint16_t segment = start;

    // Check if codepoint is in this segment
    if (codepoint < cmap->start_code[segment]) {
        return 0; // Not in this segment
    }

    // Calculate glyph index
    if (cmap->id_range_offset[segment] == 0) {
        // Simple mapping
        return (codepoint + cmap->id_delta[segment]) % 0x10000;
    } else {
        // Use glyph ID array
        uint16_t range_offset = cmap->id_range_offset[segment] / 2; // Convert from byte offset to word offset
        uint16_t glyph_array_index = range_offset + (codepoint - cmap->start_code[segment]);
        uint16_t glyph_index = cmap->glyph_id_array[glyph_array_index];

        if (glyph_index != 0) {
            return (glyph_index + cmap->id_delta[segment]) % 0x10000;
        } else {
            return 0;
        }
    }
}

// Simple hash function for glyph cache
static uint32_t glyph_hash(uint16_t glyph_index) {
    return glyph_index % GLYPH_CACHE_SIZE;
}

int ttf_render_glyph(ttf_font_t *font, uint16_t glyph_index, uint8_t *bitmap, int width, int height, int x, int y, int pixel_size) {
    if (!font || !bitmap || width <= 0 || height <= 0 || width > 64 || height > 64) {
        return -1;
    }

    // Validate glyph index
    if (glyph_index >= font->num_glyphs) {
        glyph_index = 0; // Use missing glyph
    }

    // Check cache first
    uint32_t cache_index = glyph_hash(glyph_index);
    if (font->glyph_cache[cache_index].valid &&
        font->glyph_cache[cache_index].glyph_index == glyph_index &&
        font->glyph_cache[cache_index].width == width &&
        font->glyph_cache[cache_index].height == height) {

        // Copy from cache
        memcpy(bitmap, font->glyph_cache[cache_index].bitmap, width * height);
        return 0;
    }

    // Clear the bitmap area
    memset(bitmap, 0, width * height);

    // Create a simple pattern based on glyph index
    // This is a placeholder - real implementation would parse glyf table
    int center_x = width / 2;
    int center_y = height / 2;

    // Draw bolder, better-aligned glyphs
    if (glyph_index == ' ') {
        // Space - empty
    } else if (glyph_index == 'H') {
        // H - vertical lines with horizontal connector
        for (int i = 1; i < height - 1; i++) {
            bitmap[i * width + 2] = 255;  // Left vertical
            bitmap[i * width + width - 3] = 255;  // Right vertical
            if (i == center_y - 1 || i == center_y || i == center_y + 1) {
                bitmap[i * width + center_x - 1] = 255;  // Horizontal connector
                bitmap[i * width + center_x] = 255;
                bitmap[i * width + center_x + 1] = 255;
            }
        }
    } else if (glyph_index == 'e') {
        // e - curved shape
        for (int i = 2; i < width - 2; i++) {
            bitmap[center_y * width + i] = 255;  // Middle horizontal
            if (i >= center_x - 2) {
                bitmap[(center_y + 1) * width + i] = 255;  // Bottom curve
            }
        }
        for (int i = center_y - 1; i <= center_y + 1; i++) {
            bitmap[i * width + 2] = 255;  // Left vertical
        }
    } else if (glyph_index == 'l') {
        // l - simple vertical line
        for (int i = 2; i < height - 2; i++) {
            bitmap[i * width + center_x] = 255;
            bitmap[i * width + center_x + 1] = 255;  // Make it bolder
        }
    } else if (glyph_index == 'o') {
        // o - circle/oval shape
        for (int i = center_y - 2; i <= center_y + 2; i++) {
            for (int j = center_x - 2; j <= center_x + 2; j++) {
                if ((i - center_y) * (i - center_y) + (j - center_x) * (j - center_x) <= 6) {
                    bitmap[i * width + j] = 255;
                }
            }
        }
    } else {
        // Default: draw a bolder block pattern
        int block_size = width / 3;
        for (int by = 0; by < 3; by++) {
            for (int bx = 0; bx < 3; bx++) {
                if ((glyph_index + by * 3 + bx) % 2 == 0) {
                    for (int i = by * block_size; i < (by + 1) * block_size && i < height; i++) {
                        for (int j = bx * block_size; j < (bx + 1) * block_size && j < width; j++) {
                            bitmap[i * width + j] = 255;
                        }
                    }
                }
            }
        }
    }

    // Cache the rendered glyph
    font->glyph_cache[cache_index].glyph_index = glyph_index;
    font->glyph_cache[cache_index].width = width;
    font->glyph_cache[cache_index].height = height;
    font->glyph_cache[cache_index].valid = 1;
    memcpy(font->glyph_cache[cache_index].bitmap, bitmap, width * height);

    return 0;
}