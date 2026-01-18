#pragma once
#include <stdint.h>
#include <stddef.h>

// TTF file format structures
typedef struct {
    uint32_t scaler_type;     // 0x00010000 for version 1.0
    uint16_t num_tables;      // Number of tables
    uint16_t search_range;    // (Maximum power of 2 <= numTables) * 16
    uint16_t entry_selector;  // Log2(maximum power of 2 <= numTables)
    uint16_t range_shift;     // NumTables * 16 - searchRange
} ttf_offset_table_t;

typedef struct {
    uint32_t tag;             // 4-byte identifier
    uint32_t check_sum;       // Checksum for this table
    uint32_t offset;          // Offset from beginning of file
    uint32_t length;          // Length of this table
} ttf_table_directory_t;

typedef struct {
    uint16_t major_version;
    uint16_t minor_version;
    int32_t font_revision;
    uint32_t check_sum_adjustment;
    uint32_t magic_number;
    uint16_t flags;
    uint16_t units_per_em;
    int64_t created;
    int64_t modified;
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;
    uint16_t mac_style;
    uint16_t lowest_rec_ppem;
    int16_t font_direction_hint;
    int16_t index_to_loc_format;
    int16_t glyph_data_format;
} ttf_head_table_t;

// CMAP table structures
typedef struct {
    uint16_t platform_id;
    uint16_t encoding_id;
    uint32_t offset;
} ttf_cmap_encoding_t;

typedef struct {
    uint16_t version;
    uint16_t num_encodings;
    ttf_cmap_encoding_t encodings[1]; // Variable length
} ttf_cmap_header_t;

typedef struct {
    uint16_t format;
    uint16_t length;
    uint16_t language;
    uint16_t seg_count_x2;
    uint16_t search_range;
    uint16_t entry_selector;
    uint16_t range_shift;
    uint16_t *end_code;
    uint16_t reserved_pad;
    uint16_t *start_code;
    int16_t *id_delta;
    uint16_t *id_range_offset;
    uint16_t *glyph_id_array;
} ttf_cmap_format4_t;

// Glyph header (glyf table)
typedef struct {
    int16_t number_of_contours;
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;
} ttf_glyph_header_t;

// Horizontal metrics (hmtx table)
typedef struct {
    uint16_t advance_width;
    int16_t left_side_bearing;
} ttf_long_hor_metric_t;

// Glyph outline point
typedef struct {
    int32_t x;
    int32_t y;
    uint8_t on_curve;
} ttf_point_t;

// Glyph outline
typedef struct {
    ttf_point_t *points;
    uint16_t *contours;
    int16_t num_points;
    int16_t num_contours;
} ttf_glyph_outline_t;

#define GLYPH_CACHE_SIZE 256
#define GLYPH_BITMAP_SIZE 64  // 8x8 pixels

// Cached glyph structure
typedef struct {
    uint16_t glyph_index;
    uint8_t bitmap[GLYPH_BITMAP_SIZE];
    int width;
    int height;
    int valid;
} cached_glyph_t;

// Font structure
typedef struct {
    uint8_t *font_data;
    size_t font_size;
    ttf_offset_table_t offset_table;
    ttf_table_directory_t *table_directory;
    ttf_head_table_t *head_table;
    uint16_t num_glyphs;
    uint16_t units_per_em;

    // CMAP data
    ttf_cmap_format4_t *cmap_format4;

    // Glyph location (loca) data
    uint32_t *loca_table;

    // Horizontal metrics (hmtx) data
    ttf_long_hor_metric_t *hmtx_table;
    int16_t *hmtx_left_side_bearings;
    // Glyph cache
    cached_glyph_t glyph_cache[GLYPH_CACHE_SIZE];
} ttf_font_t;

// Function declarations
int ttf_parse_font(ttf_font_t *font);
int ttf_load_font(const char *filename, ttf_font_t *font);
int ttf_load_font_data(const uint8_t *data, size_t size, ttf_font_t *font);
void ttf_free_font(ttf_font_t *font);
int ttf_get_glyph_index(ttf_font_t *font, uint32_t codepoint);
int ttf_render_glyph(ttf_font_t *font, uint16_t glyph_index, uint8_t *bitmap, int width, int height, int x, int y, int pixel_size);