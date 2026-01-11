#!/usr/bin/env python3
"""
Precise TTF-to-16x16 bitmap font converter for Tiny64 kernel.
Generates clean aligned glyphs for ASCII 32-127.
"""

import sys
import os
from PIL import Image, ImageFont, ImageDraw

def char_to_bitmap(font_path, char, size=16):
    """Render char (centered, antialiased) into 16x16 1bpp bitmap (MSB=leftmost)."""
    # Use slightly larger image and hinting to avoid glyph clipping/truncation
    pad = max(2, size // 8)
    img = Image.new('L', (size + 2*pad, size + 2*pad), 255)
    draw = ImageDraw.Draw(img)
    # Try to fit glyph vertically as well; adjust anchor if necessary
    font = ImageFont.truetype(font_path, size)
    bbox = font.getbbox(char)
    glyph_w, glyph_h = bbox[2] - bbox[0], bbox[3] - bbox[1]

    # Center text in padded box
    x = pad + (size - glyph_w) // 2 - bbox[0]
    y = pad + (size - glyph_h) // 2 - bbox[1]
    draw.text((x, y), char, font=font, fill=0)

    # Crop back to 16x16 and threshold for crisp outlines
    img = img.crop((pad, pad, pad + size, pad + size))
    img = img.point(lambda p: 0 if p < 128 else 255, mode='1')

    # Convert to 16 rows of 16 bits, MSB left
    bitmap = []
    pixels = list(img.getdata())
    for row in range(size):
        val = 0
        for col in range(size):
            if pixels[row * size + col] == 0:
                val |= (1 << (15 - col))
        bitmap.append(val)
    return bitmap

def generate_font_c(font_path, output_path):
    """Generate font.c for ASCII 32-127 from a TTF file, 16x16 bitmaps."""
    chars = [chr(i) for i in range(32, 128)]

    font_data = {}
    for char in chars:
        bitmap = char_to_bitmap(font_path, char)
        font_data[char] = bitmap

    with open(output_path, 'w') as f:
        f.write('#include "../include/kernel.h"\n\n')
        f.write('/* 16x16 font generated from TTF for printable ASCII 32..127 */\n\n')

        for i, char in enumerate(chars):
            safe = char.replace('\\', '\\\\').replace('"', '\\"').replace("'", "\\'")
            f.write(f'/* {ord(char):3d} (\'{safe}\') */\n')
            f.write(f'static const uint16_t inter_font_{i+32}[] = {{\n')
            for row in font_data[char]:
                f.write(f'    0x{row:04X},\n')
            f.write('};\n\n')

        f.write('/* Main font table: font16x16[0]=space, 95=DEL */\n')
        f.write('const uint16_t* font16x16[96] = {\n')
        for i in range(96):
            comma = ',' if i < 95 else ''
            f.write(f'    inter_font_{i+32}{comma}\n')
        f.write('};\n')

    print(f"Font generation complete: {output_path}")

def main():
    if len(sys.argv) != 3:
        print("Usage: python convert_font.py <input.ttf> <output.c>")
        sys.exit(1)
    font_path, output_path = sys.argv[1], sys.argv[2]
    if not os.path.exists(font_path):
        print(f"Error: {font_path} not found")
        sys.exit(1)
    generate_font_c(font_path, output_path)

if __name__ == "__main__":
    main()