#include "../include/kernel.h"

// Placeholder font data - this should be replaced with actual Inter.ttf data
// For now, we'll use a minimal TTF-like structure to avoid crashes
const unsigned char inter_font_data[] = {
    // Minimal TTF header (simplified)
    0x00, 0x01, 0x00, 0x00,  // sfnt version
    0x00, 0x01,              // numTables
    0x00, 0x10,              // searchRange
    0x00, 0x04,              // entrySelector
    0x00, 0x00,              // rangeShift

    // Table directory entry for 'head'
    0x68, 0x65, 0x61, 0x64,  // tag 'head'
    0x00, 0x00, 0x00, 0x00,  // checkSum
    0x00, 0x00, 0x00, 0x36,  // offset
    0x00, 0x00, 0x00, 0x36,  // length

    // head table (minimal)
    0x00, 0x01,              // majorVersion
    0x00, 0x00,              // minorVersion
    0x00, 0x00, 0x00, 0x00,  // fontRevision
    0x00, 0x00, 0x00, 0x00,  // checkSumAdjustment
    0x5F, 0x0F, 0x3C, 0xF5,  // magicNumber
    0x00, 0x00,              // flags
    0x03, 0xE8,              // unitsPerEm (1000)
    0x00, 0x00, 0x00, 0x00,  // created (low)
    0x00, 0x00, 0x00, 0x00,  // created (high)
    0x00, 0x00, 0x00, 0x00,  // modified (low)
    0x00, 0x00, 0x00, 0x00,  // modified (high)
    0x00, 0x00,              // xMin
    0x00, 0x00,              // yMin
    0x00, 0x00,              // xMax
    0x00, 0x00,              // yMax
    0x00, 0x00,              // macStyle
    0x00, 0x00,              // lowestRecPPEM
    0x00, 0x00,              // fontDirectionHint
    0x00, 0x00,              // indexToLocFormat
    0x00, 0x00               // glyphDataFormat
};

const size_t inter_font_size = sizeof(inter_font_data);
