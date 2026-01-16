#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

SRC_BOOT="$PROJECT_ROOT/boot"
SRC_KERNEL="$PROJECT_ROOT/kernel"
SRC_CORE="$SRC_KERNEL/core"
SRC_GRAPHICS="$SRC_KERNEL/graphics"
SRC_FS="$SRC_KERNEL/fs"
SRC_DOOM="$SRC_KERNEL/doom"
SRC_STUBS="$SRC_KERNEL/stubs"
SRC_DRIVERS="$SRC_KERNEL/drivers"
SRC_HAL="$PROJECT_ROOT/hal"
SRC_INCLUDE="$PROJECT_ROOT/include"
BIN="$PROJECT_ROOT/bin"
ISO="$PROJECT_ROOT/iso_root"

# Create bin directory if it doesn't exist
mkdir -p "$BIN"

GCC_FLAGS="-I$SRC_INCLUDE -I$SRC_HAL -I$SRC_DRIVERS -ffreestanding -mno-red-zone -fno-stack-protector -fno-pie -msse2 -c"

echo "[0] BIOS Bootloader Assembly"

# Assemble MBR
as --32 -o "$BIN/mbr.o" "$SRC_BOOT/mbr.S"
ld -m elf_i386 -Ttext=0x7C00 -o "$BIN/mbr.elf" "$BIN/mbr.o"
objcopy -O binary "$BIN/mbr.elf" "$BIN/mbr.bin"

# Assemble Stage 1
as --32 -o "$BIN/stage1.o" "$SRC_BOOT/stage1.S"
ld -m elf_i386 -Ttext=0x7E00 -o "$BIN/stage1.elf" "$BIN/stage1.o"
objcopy -O binary "$BIN/stage1.elf" "$BIN/stage1.bin"

# Assemble Stage 2
as --32 -o "$BIN/stage2.o" "$SRC_BOOT/stage2.S"
ld -m elf_i386 -Ttext=0x8000 -o "$BIN/stage2.elf" "$BIN/stage2.o"
objcopy -O binary "$BIN/stage2.elf" "$BIN/stage2.bin"

echo "[1] Kernel Build"

# Build kernel (same as UEFI build)
OBJ_FILES=()

# BIOS boot initialization
BIOS_INIT_OBJ="$BIN/bios_init.o"
gcc $GCC_FLAGS "$SRC_BOOT/bios_init.c" -o "$BIOS_INIT_OBJ"
OBJ_FILES+=("$BIOS_INIT_OBJ")

# BIOS stubs for missing functions
BIOS_STUBS_OBJ="$BIN/bios_stubs.o"
gcc $GCC_FLAGS "$SRC_BOOT/bios_stubs.c" -o "$BIOS_STUBS_OBJ"
OBJ_FILES+=("$BIOS_STUBS_OBJ")

# Compile kernel core
ENTRY_SRC="$SRC_CORE/entry.S"
ENTRY_OBJ="$BIN/entry.o"
gcc $GCC_FLAGS "$ENTRY_SRC" -o "$ENTRY_OBJ"
OBJ_FILES+=("$ENTRY_OBJ")

# Graphics
for src in "$SRC_GRAPHICS/graphics.c" "$SRC_GRAPHICS/font.c" "$SRC_FS/memory.c" "$SRC_FS/fs.c" "$SRC_STUBS/string.c" "$SRC_GRAPHICS/ttf.c" "$SRC_GRAPHICS/inter_font_data.c" "$SRC_GRAPHICS/winxp_ui.c" "$SRC_CORE/kernel.c" "$SRC_DOOM/m_argv.c" "$SRC_STUBS/system_stubs.c" "$SRC_STUBS/doom_stubs.c" "$SRC_DOOM/d_main.c" "$SRC_DOOM/doomgeneric_tiny64.c"; do
    if [[ "$src" != "$SRC_STUBS/kernel_stubs.c" && "$src" != "$SRC_STUBS/doomgeneric_stubs.c" ]]; then
        obj="$BIN/$(basename "$src" | sed 's/\.\w\+$/.o/')"
        gcc $GCC_FLAGS "$src" -o "$obj"
        OBJ_FILES+=("$obj")
    fi
done

# Kernel stubs
for src in "$SRC_STUBS/kernel_stubs.c" "$SRC_STUBS/doomgeneric_stubs.c"; do
    obj="$BIN/$(basename "$src" | sed 's/\.\w\+$/.o/')"
    gcc $GCC_FLAGS "$src" -o "$obj"
    OBJ_FILES+=("$obj")
done

# Add remaining kernel components
add_objs_from_dir_exclude() {
    local dir="$1"
    local filter_entry="$2"
    for src in $(find "$dir" -maxdepth 1 -type f \( -name "*.c" -o -name "*.S" -o -name "*.s" \) | sort); do
        [[ "$filter_entry" = "yes" && "$(basename "$src")" == "entry.S" ]] && continue
        if [[ "$dir" == "$SRC_CORE" && ( "$(basename "$src")" == "recovery.c" || "$(basename "$src")" == "kernel.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_GRAPHICS" && ( "$(basename "$src")" == "graphics.c" || "$(basename "$src")" == "font.c" || "$(basename "$src")" == "ttf.c" || "$(basename "$src")" == "inter_font_data.c" || "$(basename "$src")" == "winxp_ui.c" || "$(basename "$src")" == "better_font.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_FS" && ( "$(basename "$src")" == "memory.c" || "$(basename "$src")" == "fs.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_STUBS" && ( "$(basename "$src")" == "string.c" || "$(basename "$src")" == "system_stubs.c" || "$(basename "$src")" == "doom_stubs.c" || "$(basename "$src")" == "kernel_stubs.c" || "$(basename "$src")" == "doomgeneric_stubs.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_DOOM" && ( "$(basename "$src")" == "m_argv.c" || "$(basename "$src")" == "d_main.c" || "$(basename "$src")" == "doomgeneric_tiny64.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_HAL" && "$(basename "$src")" == "serial.c" ]]; then continue; fi
        if [[ "$dir" == "$SRC_DRIVERS" ]]; then continue; fi
        obj="$BIN/$(basename "$src" | sed 's/\.\w\+$/.o/')"
        gcc $GCC_FLAGS "$src" -o "$obj"
        OBJ_FILES+=("$obj")
    done
}

add_objs_from_dir_exclude "$SRC_CORE" yes
add_objs_from_dir_exclude "$SRC_GRAPHICS" yes
add_objs_from_dir_exclude "$SRC_FS" yes
add_objs_from_dir_exclude "$SRC_DOOM" yes
add_objs_from_dir_exclude "$SRC_STUBS" yes

# Serial and drivers
SERIAL_OBJ="$BIN/serial.o"
gcc $GCC_FLAGS "$SRC_HAL/serial.c" -o "$SERIAL_OBJ"
OBJ_FILES+=("$SERIAL_OBJ")

# Drivers
DRIVER_GCC_FLAGS="$GCC_FLAGS -I$SRC_STUBS -I$SRC_CORE"
cd "$SRC_DRIVERS"
USB_OBJ="$BIN/usb.o"
gcc $DRIVER_GCC_FLAGS "usb.c" -o "$USB_OBJ"
OBJ_FILES+=("$USB_OBJ")

RTL8139_OBJ="$BIN/rtl8139.o"
gcc $DRIVER_GCC_FLAGS "rtl8139.c" -o "$RTL8139_OBJ"
OBJ_FILES+=("$RTL8139_OBJ")

AC97_OBJ="$BIN/ac97.o"
gcc $DRIVER_GCC_FLAGS "ac97.c" -o "$AC97_OBJ"
OBJ_FILES+=("$AC97_OBJ")

IDE_OBJ="$BIN/ide.o"
gcc $DRIVER_GCC_FLAGS "ide.c" -o "$IDE_OBJ"
OBJ_FILES+=("$IDE_OBJ")
cd "$PROJECT_ROOT"

# WAD embedding
if [ -f "$PROJECT_ROOT/doom1.wad" ]; then
    echo "Embedding doom1.wad..."
    WAD_SIZE=$(stat -c %s "$PROJECT_ROOT/doom1.wad")
    MAX_SIZE=1048576
    if [ $WAD_SIZE -gt $MAX_SIZE ]; then
        WAD_SIZE=$MAX_SIZE
    fi

    echo "#ifndef EMBEDDED_WAD_H" > "$BIN/embedded_wad.h"
    echo "#define EMBEDDED_WAD_H" >> "$BIN/embedded_wad.h"
    echo "#include <stdint.h>" >> "$BIN/embedded_wad.h"
    echo "#include <stddef.h>" >> "$BIN/embedded_wad.h"
    echo "extern const uint8_t doom1_wad_data[];" >> "$BIN/embedded_wad.h"
    echo "extern const size_t doom1_wad_size;" >> "$BIN/embedded_wad.h"
    echo "const uint8_t* get_doom1_wad_data(size_t* size);" >> "$BIN/embedded_wad.h"
    echo "const uint8_t* get_doom_wad_data(size_t* size);" >> "$BIN/embedded_wad.h"
    echo "const uint8_t* get_doom2_wad_data(size_t* size);" >> "$BIN/embedded_wad.h"
    echo "#endif" >> "$BIN/embedded_wad.h"

    echo "#include \"embedded_wad.h\"" > "$BIN/embedded_wad.c"
    echo "const uint8_t doom1_wad_data[] = {" >> "$BIN/embedded_wad.c"
    head -c $WAD_SIZE "$PROJECT_ROOT/doom1.wad" | xxd -i >> "$BIN/embedded_wad.c"
    sed -i 's/unsigned char [^[]*\[/const uint8_t doom1_wad_data[/g' "$BIN/embedded_wad.c"
    echo "};" >> "$BIN/embedded_wad.c"
    echo "const size_t doom1_wad_size = $WAD_SIZE;" >> "$BIN/embedded_wad.c"
    echo "const uint8_t* get_doom1_wad_data(size_t* size) { *size = doom1_wad_size; return doom1_wad_data; }" >> "$BIN/embedded_wad.c"
    echo "const uint8_t* get_doom_wad_data(size_t* size) { *size = 0; return NULL; }" >> "$BIN/embedded_wad.c"
    echo "const uint8_t* get_doom2_wad_data(size_t* size) { *size = 0; return NULL; }" >> "$BIN/embedded_wad.c"

    gcc $GCC_FLAGS -c "$BIN/embedded_wad.c" -o "$BIN/embedded_wad.o"
    OBJ_FILES+=("$BIN/embedded_wad.o")
fi

echo "[2] Link Kernel"
ld -T "$SRC_CORE/link_kernel.ld" -o "$BIN/kernel.elf" "${OBJ_FILES[@]}"
objcopy -O binary "$BIN/kernel.elf" "$BIN/kernel.bin"

echo "[3] Create BIOS Disk Image"

# Create 16MB disk image (enough for kernel + potential growth)
dd if=/dev/zero of="$BIN/tiny64_bios.img" bs=1M count=16

# Write MBR (512 bytes) to sector 0
dd if="$BIN/mbr.bin" of="$BIN/tiny64_bios.img" bs=512 count=1 conv=notrunc

# Write Stage 1 (512 bytes) to sector 1
dd if="$BIN/stage1.bin" of="$BIN/tiny64_bios.img" bs=512 seek=1 count=1 conv=notrunc

# Write Stage 2 (2048 bytes, 4 sectors) to sectors 2-5
dd if="$BIN/stage2.bin" of="$BIN/tiny64_bios.img" bs=512 seek=2 count=4 conv=notrunc

# Write kernel to sectors 6+ (starting at 0x100000 in memory)
# Calculate sectors needed (rounded up)
KERNEL_SIZE=$(stat -c %s "$BIN/kernel.bin")
KERNEL_SECTORS=$(((KERNEL_SIZE + 511) / 512))
echo "Kernel size: $KERNEL_SIZE bytes ($KERNEL_SECTORS sectors)"
dd if="$BIN/kernel.bin" of="$BIN/tiny64_bios.img" bs=512 seek=6 conv=notrunc

echo "[4] BIOS Boot Image Ready"
echo "BIOS disk image: $BIN/tiny64_bios.img"
echo "Ready for testing with QEMU: qemu-system-x86_64 -drive file=$BIN/tiny64_bios.img,format=raw"
