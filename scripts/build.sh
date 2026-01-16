#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

SRC_BOOT="$PROJECT_ROOT/boot"
SRC_KERNEL="$PROJECT_ROOT/kernel"
SRC_CORE="$SRC_KERNEL/core"
SRC_GRAPHICS="$SRC_KERNEL/graphics"
SRC_FS="$SRC_KERNEL/fs"
SRC_DOOM="$SRC_KERNEL/doom"
SRC_STUBS="$SRC_KERNEL/stubs"
SRC_HAL="$PROJECT_ROOT/hal"
SRC_DRIVERS="$SRC_KERNEL/drivers"
SRC_INCLUDE="$PROJECT_ROOT/include"
BIN="$PROJECT_ROOT/bin"
ISO="$PROJECT_ROOT/iso_root"

FAT32_IMG="Tiny64.fat32"
ISO_IMG="Tiny64.iso"
EFI_IMG="efiboot.img"

rm -rf "$BIN" "$ISO" "$ISO_IMG" "$EFI_IMG" "$FAT32_IMG"
mkdir -p "$BIN"
mkdir -p "$ISO/EFI/BOOT"

echo "[0] Bootloader"
x86_64-w64-mingw32-gcc -I. -ffreestanding -mno-red-zone -nostdlib -Wall \
    -Wl,--subsystem,10 -e EfiMain \
    -o "$ISO/EFI/BOOT/BOOTX64.EFI" \
    "$SRC_BOOT/boot.c"

echo "[2] Kernel"
GCC_FLAGS="-I$SRC_INCLUDE -I$SRC_HAL -I$SRC_DRIVERS -ffreestanding -mno-red-zone -fno-stack-protector -fno-pie -msse2 -c"
OBJ_FILES=()

# Embed WAD file after OBJ_FILES initialization
echo "[2.1] Embed WAD file"
if [ -f "$PROJECT_ROOT/doom1.wad" ]; then
    echo "Embedding doom1.wad..."
    # Create a header file with embedded WAD data
    echo "#ifndef EMBEDDED_WAD_H" > "$BIN/embedded_wad.h"
    echo "#define EMBEDDED_WAD_H" >> "$BIN/embedded_wad.h"
    echo "" >> "$BIN/embedded_wad.h"
    echo "#include <stdint.h>" >> "$BIN/embedded_wad.h"
    echo "#include <stddef.h>" >> "$BIN/embedded_wad.h"
    echo "" >> "$BIN/embedded_wad.h"
    echo "// Embedded doom1.wad data" >> "$BIN/embedded_wad.h"
    echo "extern const uint8_t doom1_wad_data[];" >> "$BIN/embedded_wad.h"
    echo "extern const size_t doom1_wad_size;" >> "$BIN/embedded_wad.h"
    echo "" >> "$BIN/embedded_wad.h"
    echo "// Function to access the data" >> "$BIN/embedded_wad.h"
    echo "const uint8_t* get_doom1_wad_data(size_t* size);" >> "$BIN/embedded_wad.h"
    echo "" >> "$BIN/embedded_wad.h"
    echo "#endif" >> "$BIN/embedded_wad.h"

    # Create the C file with the actual data
    WAD_SIZE=$(stat -c %s "$PROJECT_ROOT/doom1.wad")
    echo "#include \"embedded_wad.h\"" > "$BIN/embedded_wad.c"
    echo "" >> "$BIN/embedded_wad.c"
    echo "// Embedded doom1.wad data - generated from binary file" >> "$BIN/embedded_wad.c"
    echo "const uint8_t doom1_wad_data[] = {" >> "$BIN/embedded_wad.c"

    # Convert binary file to hex array (limit size for compilation speed)
    MAX_SIZE=1048576  # 1MB limit for reasonable compilation time
    if [ $WAD_SIZE -gt $MAX_SIZE ]; then
        echo "Warning: WAD file is large ($WAD_SIZE bytes), truncating to $MAX_SIZE bytes"
        WAD_SIZE=$MAX_SIZE
    fi

    # Use xxd to convert to C array format (much more reliable than od + sed)
    head -c $WAD_SIZE "$PROJECT_ROOT/doom1.wad" | xxd -i >> "$BIN/embedded_wad.c"

    # xxd creates a variable name like unsigned_char_PROJECT_ROOT_doom1_wad_wad[]
    # We need to rename it to doom1_wad_data
    sed -i 's/unsigned char [^[]*\[/const uint8_t doom1_wad_data[/g' "$BIN/embedded_wad.c"

    echo "};" >> "$BIN/embedded_wad.c"
    echo "" >> "$BIN/embedded_wad.c"
    echo "const size_t doom1_wad_size = $WAD_SIZE;" >> "$BIN/embedded_wad.c"
    echo "" >> "$BIN/embedded_wad.c"
    echo "const uint8_t* get_doom1_wad_data(size_t* size) {" >> "$BIN/embedded_wad.c"
    echo "    *size = doom1_wad_size;" >> "$BIN/embedded_wad.c"
    echo "    return doom1_wad_data;" >> "$BIN/embedded_wad.c"
    echo "}" >> "$BIN/embedded_wad.c"
    echo "" >> "$BIN/embedded_wad.c"
    echo "// Stub implementations for other WAD files (not embedded)" >> "$BIN/embedded_wad.c"
    echo "const uint8_t* get_doom_wad_data(size_t* size) {" >> "$BIN/embedded_wad.c"
    echo "    *size = 0;" >> "$BIN/embedded_wad.c"
    echo "    return NULL;" >> "$BIN/embedded_wad.c"
    echo "}" >> "$BIN/embedded_wad.c"
    echo "" >> "$BIN/embedded_wad.c"
    echo "const uint8_t* get_doom2_wad_data(size_t* size) {" >> "$BIN/embedded_wad.c"
    echo "    *size = 0;" >> "$BIN/embedded_wad.c"
    echo "    return NULL;" >> "$BIN/embedded_wad.c"
    echo "}" >> "$BIN/embedded_wad.c"

    # Compile the embedded WAD data
    gcc $GCC_FLAGS -c "$BIN/embedded_wad.c" -o "$BIN/embedded_wad.o"
    if [ -f "$BIN/embedded_wad.o" ]; then
        OBJ_FILES+=("$BIN/embedded_wad.o")
        echo "WAD file embedded successfully (size: $WAD_SIZE bytes)"
    else
        echo "ERROR: Failed to compile embedded WAD"
    fi
elif [ -f "$PROJECT_ROOT/doom.wad" ]; then
    echo "Embedding doom.wad..."
    objcopy -I binary -O elf64-x86-64 --binary-architecture i386 --rename-section .data=.rodata,alloc,load,readonly,data,contents "$PROJECT_ROOT/doom.wad" "$BIN/doom_wad.o"
    OBJ_FILES+=("$BIN/doom_wad.o")
    echo "WAD file embedded successfully"
elif [ -f "$PROJECT_ROOT/doom2.wad" ]; then
    echo "Embedding doom2.wad..."
    objcopy -I binary -O elf64-x86-64 --binary-architecture i386 --rename-section .data=.rodata,alloc,load,readonly,data,contents "$PROJECT_ROOT/doom2.wad" "$BIN/doom2_wad.o"
    OBJ_FILES+=("$BIN/doom2_wad.o")
    echo "WAD file embedded successfully"
else
    echo "No WAD file found, Doom will not work"
fi

ENTRY_SRC="$SRC_CORE/entry.S"
ENTRY_OBJ="$BIN/entry.o"
gcc $GCC_FLAGS "$ENTRY_SRC" -o "$ENTRY_OBJ"
OBJ_FILES+=("$ENTRY_OBJ")

for src in "$SRC_GRAPHICS/graphics.c" "$SRC_GRAPHICS/font.c" "$SRC_FS/memory.c" "$SRC_FS/fs.c" "$SRC_STUBS/string.c" "$SRC_GRAPHICS/ttf.c" "$SRC_GRAPHICS/inter_font_data.c" "$SRC_GRAPHICS/winxp_ui.c" "$SRC_CORE/kernel.c" "$SRC_DOOM/m_argv.c" "$SRC_STUBS/system_stubs.c" "$SRC_STUBS/doom_stubs.c" "$SRC_DOOM/d_main.c" "$SRC_DOOM/doomgeneric_tiny64.c"; do
    obj="$BIN/$(basename "$src" .c).o"
    # Avoid compiling kernel_stubs.c and doomgeneric_stubs.c here
    if [[ "$src" != "$SRC_STUBS/kernel_stubs.c" && "$src" != "$SRC_STUBS/doomgeneric_stubs.c" ]]; then
        gcc $GCC_FLAGS "$src" -o "$obj"
        OBJ_FILES+=("$obj")
    fi
done
# Add kernel_stubs.c and doomgeneric_stubs.c exactly once
for src in "$SRC_STUBS/kernel_stubs.c" "$SRC_STUBS/doomgeneric_stubs.c"; do
    obj="$BIN/$(basename "$src" .c).o"
    gcc $GCC_FLAGS "$src" -o "$obj"
    OBJ_FILES+=("$obj")
done

function add_objs_from_dir_exclude() {
    local dir="$1"
    local filter_entry="$2"
    for src in $(find "$dir" -maxdepth 1 -type f \( -name "*.c" -o -name "*.S" -o -name "*.s" \) | sort); do
        [[ "$filter_entry" = "yes" && "$(basename "$src")" == "entry.S" ]] && continue
        # Skip files that were already compiled manually above
        if [[ "$dir" == "$SRC_CORE" && ( "$(basename "$src")" == "recovery.c" || "$(basename "$src")" == "kernel.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_GRAPHICS" && ( "$(basename "$src")" == "graphics.c" || "$(basename "$src")" == "font.c" || "$(basename "$src")" == "ttf.c" || "$(basename "$src")" == "inter_font_data.c" || "$(basename "$src")" == "winxp_ui.c" || "$(basename "$src")" == "better_font.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_FS" && ( "$(basename "$src")" == "memory.c" || "$(basename "$src")" == "fs.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_STUBS" && ( "$(basename "$src")" == "string.c" || "$(basename "$src")" == "system_stubs.c" || "$(basename "$src")" == "doom_stubs.c" || "$(basename "$src")" == "kernel_stubs.c" || "$(basename "$src")" == "doomgeneric_stubs.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_DOOM" && ( "$(basename "$src")" == "m_argv.c" || "$(basename "$src")" == "d_main.c" || "$(basename "$src")" == "doomgeneric_tiny64.c" ) ]]; then continue; fi
        if [[ "$dir" == "$SRC_HAL" && "$(basename "$src")" == "serial.c" ]]; then continue; fi
        if [[ "$dir" == "$SRC_DRIVERS" ]]; then continue; fi # Drivers are not auto-compiled
        obj="$BIN/$(basename "$src" | sed 's/\.\w\+$/.o/')"
        gcc $GCC_FLAGS "$src" -o "$obj"
        OBJ_FILES+=("$obj")
    done
}

SERIAL_SRC="$SRC_HAL/serial.c"
SERIAL_OBJ="$BIN/serial.o"
gcc $GCC_FLAGS "$SERIAL_SRC" -o "$SERIAL_OBJ"
OBJ_FILES+=("$SERIAL_OBJ")

# Compile drivers manually (with proper include paths)
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

add_objs_from_dir_exclude "$SRC_HAL" no
add_objs_from_dir_exclude "$SRC_CORE" yes
add_objs_from_dir_exclude "$SRC_GRAPHICS" yes
add_objs_from_dir_exclude "$SRC_FS" yes
add_objs_from_dir_exclude "$SRC_DOOM" yes
add_objs_from_dir_exclude "$SRC_STUBS" yes

echo "[2.5] Link Kernel"
ld -T "$SRC_CORE/link_kernel.ld" -o "$BIN/kernel.elf" "${OBJ_FILES[@]}"
objcopy -O binary "$BIN/kernel.elf" "$ISO/kernel.t64"

echo "[3] Recovery Kernel"
OBJ_RECOVERY=()
OBJ_RECOVERY+=("$ENTRY_OBJ")

for src in $(find "$SRC_HAL" -maxdepth 1 -type f \( -name "*.c" -o -name "*.S" -o -name "*.s" \) | sort); do
    [[ "$(basename "$src")" == "serial.c" ]] && continue
    obj="$BIN/recovery_$(basename "$src" | sed 's/\.\w\+$/.o/')"
    gcc $GCC_FLAGS "$src" -o "$obj"
    OBJ_RECOVERY+=("$obj")
done

RECOVERY_C="$SRC_CORE/recovery.c"
RECOVERY_OBJ="$BIN/recovery.o"
gcc $GCC_FLAGS -DRECOVERY_KERNEL "$RECOVERY_C" -o "$RECOVERY_OBJ"
OBJ_RECOVERY+=("$RECOVERY_OBJ")

GRAPHICS_OBJ="$BIN/recovery_graphics.o"
gcc $GCC_FLAGS -DRECOVERY_KERNEL "$SRC_GRAPHICS/graphics.c" -o "$GRAPHICS_OBJ"
OBJ_RECOVERY+=("$GRAPHICS_OBJ")

SERIAL_RECOVERY_OBJ="$BIN/recovery_serial.o"
gcc $GCC_FLAGS "$SRC_HAL/serial.c" -o "$SERIAL_RECOVERY_OBJ"
OBJ_RECOVERY+=("$SERIAL_RECOVERY_OBJ")

STRING_RECOVERY_OBJ="$BIN/recovery_string.o"
gcc $GCC_FLAGS "$SRC_STUBS/string.c" -o "$STRING_RECOVERY_OBJ"
OBJ_RECOVERY+=("$STRING_RECOVERY_OBJ")

FONT_OBJ="$BIN/recovery_font.o"
gcc $GCC_FLAGS "$SRC_GRAPHICS/font.c" -o "$FONT_OBJ"
OBJ_RECOVERY+=("$FONT_OBJ")

for src in $(find "$SRC_DRIVERS" -maxdepth 1 -type f \( -name "*.c" -o -name "*.S" -o -name "*.s" \) | sort); do
    obj="$BIN/recovery_$(basename "$src" | sed 's/\.\w\+$/.o/')"
    gcc $GCC_FLAGS "$src" -o "$obj"
    OBJ_RECOVERY+=("$obj")
done

echo "[3.5] Link Recovery"
ld -T "$SRC_CORE/link_kernel.ld" -o "$BIN/recovery.elf" "${OBJ_RECOVERY[@]}"
objcopy -O binary "$BIN/recovery.elf" "$ISO/recovery.t64"

echo "[4] FAT32 Image"
dd if=/dev/zero of="$FAT32_IMG" bs=1M count=16 2>/dev/null
mkfs.vfat "$FAT32_IMG" >/dev/null
MTOOLS_SKIP_CHECK=1 mmd -i "$FAT32_IMG" ::/EFI
MTOOLS_SKIP_CHECK=1 mmd -i "$FAT32_IMG" ::/EFI/BOOT
MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT32_IMG" "$ISO/EFI/BOOT/BOOTX64.EFI" ::/EFI/BOOT/
MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT32_IMG" "$ISO/kernel.t64" ::
MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT32_IMG" "$ISO/recovery.t64" ::

echo "[5] EFI Image"
dd if=/dev/zero of="$EFI_IMG" bs=1M count=64 2>/dev/null
mkfs.vfat "$EFI_IMG" >/dev/null
MTOOLS_SKIP_CHECK=1 mcopy -s -i "$EFI_IMG" "$ISO/EFI" ::
MTOOLS_SKIP_CHECK=1 mcopy -i "$EFI_IMG" "$ISO/kernel.t64" ::
MTOOLS_SKIP_CHECK=1 mcopy -i "$EFI_IMG" "$ISO/recovery.t64" ::
cp "$EFI_IMG" "$ISO/"

echo "[6] ISO"
xorriso -as mkisofs -V 'TINY64' -e efiboot.img -no-emul-boot -o "$ISO_IMG" "$ISO" 2>/dev/null

echo "[7] Done"
echo "$FAT32_IMG"
echo "$ISO_IMG"
echo "[8] Launching QEMU..."
if command -v qemu-system-x86_64 >/dev/null 2>&1 && [ -c /dev/kvm ]; then
    qemu-system-x86_64 -enable-kvm -m 256M -drive file="$ISO_IMG",media=cdrom,format=raw -serial stdio -bios OVMF.fd
    else
    echo "Warning: KVM not available or QEMU missing, running without KVM acceleration."
    qemu-system-x86_64 -m 256M -drive file="$ISO_IMG",media=cdrom,format=raw -serial stdio -bios OVMF.fd
fi
