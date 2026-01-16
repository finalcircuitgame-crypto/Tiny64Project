#!/usr/bin/env python3

# Read the font.c file
with open('kernel/font.c', 'r') as f:
    lines = f.readlines()

# Find the font16x16 array definition
font_data = []
in_array = False
for line in lines:
    if 'const uint16_t* font16x16[96]' in line:
        in_array = True
        continue
    if in_array:
        if '};' in line:
            break
        # Extract the font names
        if 'inter_font_' in line:
            font_name = line.strip().replace(',', '')
            font_data.append(font_name)

print(f'Found {len(font_data)} font entries')

# Now read each font array
all_font_bytes = []
for font_name in font_data:
    # Find the array for this font
    array_start = -1
    array_end = -1
    for i, line in enumerate(lines):
        if f'static const uint16_t {font_name}[]' in line:
            array_start = i + 1
            break

    if array_start == -1:
        continue

    # Find the end of this array
    for i in range(array_start, len(lines)):
        if '};' in lines[i]:
            array_end = i
            break

    # Extract the values
    values = []
    for i in range(array_start, array_end):
        line = lines[i].strip()
        if line and not line.startswith('//'):
            # Extract hex values
            hex_vals = [v.strip() for v in line.replace(',', '').split() if v.strip().startswith('0x')]
            values.extend(hex_vals)

    # Convert to bytes (big endian)
    for val in values:
        if val.startswith('0x'):
            num = int(val, 16)
            all_font_bytes.append('0x%02X' % (num >> 8))
            all_font_bytes.append('0x%02X' % (num & 0xFF))

print(f'Total bytes collected: {len(all_font_bytes)}')

# Write the new inter_font_data.c
with open('kernel/inter_font_data.c', 'w') as f:
    f.write('#include "../include/kernel.h"\n\n')
    f.write('// Embedded bitmap font data (16x16 pixels, 96 characters, 32 bytes each)\n')
    f.write('// Characters 32-127 (space to delete)\n')
    f.write('const unsigned char inter_font_data[] = {\n')

    # Write in rows of 16 bytes for readability
    for i in range(0, len(all_font_bytes), 16):
        row = all_font_bytes[i:i+16]
        f.write('    ' + ', '.join(row) + ',\n')

    f.write('};\n\n')
    f.write('const size_t inter_font_size = sizeof(inter_font_data);\n')

print('Conversion complete')