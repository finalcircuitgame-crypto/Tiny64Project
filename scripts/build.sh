#!/usr/bin/env bash
set -e

# --- Configuration ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

SRC_BOOT="$PROJECT_ROOT/boot"
SRC_KERNEL="$PROJECT_ROOT/kernel"
SRC_HAL="$PROJECT_ROOT/hal"
SRC_DRIVERS="$PROJECT_ROOT/drivers"
SRC_INCLUDE="$PROJECT_ROOT/include"
BIN="$PROJECT_ROOT/bin"
ISO="$PROJECT_ROOT/iso_root"

FAT32_IMG="Tiny64.fat32"
ISO_IMG="Tiny64.iso"
EFI_IMG="efiboot.img"

# --- Cleanup ---
rm -rf "$BIN" "$ISO" "$ISO_IMG" "$EFI_IMG" "$FAT32_IMG"
mkdir -p "$BIN"
mkdir -p "$ISO/EFI/BOOT"

# --- 1. Compile Bootloader ---
echo "[1] Compiling Bootloader..."
x86_64-w64-mingw32-gcc -I. -ffreestanding -mno-red-zone -nostdlib -Wall \
    -Wl,--subsystem,10 -e EfiMain \
    -o "$ISO/EFI/BOOT/BOOTX64.EFI" \
    "$SRC_BOOT/boot.c"

# --- 2. Compile Main Kernel & All Modules (except recovery) ---
echo "[2] Compiling Main Kernel and Modules..."

GCC_FLAGS="-I$SRC_INCLUDE -I$SRC_HAL -ffreestanding -mno-red-zone -fno-stack-protector -fno-pie -mgeneral-regs-only -c"
OBJ_FILES=()

# 2.1 Compile Assembly Entry (always first)
ENTRY_SRC="$SRC_KERNEL/entry.S"
ENTRY_OBJ="$BIN/entry.o"
gcc $GCC_FLAGS "$ENTRY_SRC" -o "$ENTRY_OBJ"
OBJ_FILES+=("$ENTRY_OBJ")

# 2.2 Compile graphics.c, font.c, and memory.c in kernel explicitly
for src in "$SRC_KERNEL/graphics.c" "$SRC_KERNEL/font.c" "$SRC_KERNEL/memory.c"; do
    obj="$BIN/$(basename "$src" .c).o"
    gcc $GCC_FLAGS "$src" -o "$obj"
    OBJ_FILES+=("$obj")
done

# 2.3 Compile all .c and .S/.s in hal, kernel, drivers (except entry.S, recovery.c, graphics.c, font.c, serial.c), keeping deterministic order
function add_objs_from_dir_exclude() {
    local dir="$1"
    local filter_entry="$2"
    for src in $(find "$dir" -maxdepth 1 -type f \( -name "*.c" -o -name "*.S" -o -name "*.s" \) | sort); do
        # Skip entry.S if requested
        [[ "$filter_entry" = "yes" && "$(basename "$src")" == "entry.S" ]] && continue
        # Skip kernel/recovery.c, graphics.c, font.c, and memory.c for main kernel
        if [[ "$dir" == "$SRC_KERNEL" && ( "$(basename "$src")" == "recovery.c" || "$(basename "$src")" == "graphics.c" || "$(basename "$src")" == "font.c" || "$(basename "$src")" == "memory.c" ) ]]; then continue; fi
        # Skip hal/serial.c for main kernel (compiled separately)
        if [[ "$dir" == "$SRC_HAL" && "$(basename "$src")" == "serial.c" ]]; then continue; fi
        obj="$BIN/$(basename "$src" | sed 's/\.\w\+$/.o/')"
        gcc $GCC_FLAGS "$src" -o "$obj"
        OBJ_FILES+=("$obj")
    done
}

# 2.4 Compile serial.c separately for main kernel
SERIAL_SRC="$SRC_HAL/serial.c"
SERIAL_OBJ="$BIN/serial.o"
gcc $GCC_FLAGS "$SERIAL_SRC" -o "$SERIAL_OBJ"
OBJ_FILES+=("$SERIAL_OBJ")

add_objs_from_dir_exclude "$SRC_HAL" no
add_objs_from_dir_exclude "$SRC_KERNEL" yes
add_objs_from_dir_exclude "$SRC_DRIVERS" no

# --- 3. Link Main Kernel (ORDER MATTERS: entry.o is always first) ---
echo "[2.5] Linking Main Kernel..."
ld -T "$SRC_KERNEL/link_kernel.ld" -o "$BIN/kernel.elf" "${OBJ_FILES[@]}"

# Extract pure binary
objcopy -O binary "$BIN/kernel.elf" "$ISO/kernel.t64"

# --- 4. Build Recovery Kernel (Only recovery.c + all HAL/drivers + entry.o) ---
echo "[3] Compiling Recovery Kernel..."

OBJ_RECOVERY=()
OBJ_RECOVERY+=("$ENTRY_OBJ")

# HAL (excluding serial.c which is compiled separately)
for src in $(find "$SRC_HAL" -maxdepth 1 -type f \( -name "*.c" -o -name "*.S" -o -name "*.s" \) | sort); do
    # Skip serial.c for recovery kernel (compiled separately)
    [[ "$(basename "$src")" == "serial.c" ]] && continue
    obj="$BIN/recovery_$(basename "$src" | sed 's/\.\w\+$/.o/')"
    gcc $GCC_FLAGS "$src" -o "$obj"
    OBJ_RECOVERY+=("$obj")
done

# KERNEL: recovery.c + graphics functions (needed for kprint)
RECOVERY_C="$SRC_KERNEL/recovery.c"
RECOVERY_OBJ="$BIN/recovery.o"
gcc $GCC_FLAGS "$RECOVERY_C" -o "$RECOVERY_OBJ"
OBJ_RECOVERY+=("$RECOVERY_OBJ")

# Add graphics.o for kprint function
GRAPHICS_OBJ="$BIN/recovery_graphics.o"
gcc $GCC_FLAGS "$SRC_KERNEL/graphics.c" -o "$GRAPHICS_OBJ"
OBJ_RECOVERY+=("$GRAPHICS_OBJ")

# Add serial.o for serial output
SERIAL_RECOVERY_OBJ="$BIN/recovery_serial.o"
gcc $GCC_FLAGS "$SRC_HAL/serial.c" -o "$SERIAL_RECOVERY_OBJ"
OBJ_RECOVERY+=("$SERIAL_RECOVERY_OBJ")

# Add font.o for font data
FONT_OBJ="$BIN/recovery_font.o"
gcc $GCC_FLAGS "$SRC_KERNEL/font.c" -o "$FONT_OBJ"
OBJ_RECOVERY+=("$FONT_OBJ")

# DRIVERS
for src in $(find "$SRC_DRIVERS" -maxdepth 1 -type f \( -name "*.c" -o -name "*.S" -o -name "*.s" \) | sort); do
    obj="$BIN/recovery_$(basename "$src" | sed 's/\.\w\+$/.o/')"
    gcc $GCC_FLAGS "$src" -o "$obj"
    OBJ_RECOVERY+=("$obj")
done

echo "[3.5] Linking Recovery Kernel..."
ld -T "$SRC_KERNEL/link_kernel.ld" -o "$BIN/recovery.elf" "${OBJ_RECOVERY[@]}"

# Extract pure binary recovery kernel
objcopy -O binary "$BIN/recovery.elf" "$ISO/recovery.t64"

# --- 5. Build FAT32 Image ---
echo "[4] Building FAT32 image ($FAT32_IMG)..."
dd if=/dev/zero of="$FAT32_IMG" bs=1M count=16 2>/dev/null
mkfs.vfat "$FAT32_IMG" > /dev/null

# Copy files into FAT32 root (EFI/BOOT/BOOTX64.EFI, kernel.t64, recovery.t64)
MTOOLS_SKIP_CHECK=1 mmd -i "$FAT32_IMG" ::/EFI
MTOOLS_SKIP_CHECK=1 mmd -i "$FAT32_IMG" ::/EFI/BOOT
MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT32_IMG" "$ISO/EFI/BOOT/BOOTX64.EFI" ::/EFI/BOOT/
MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT32_IMG" "$ISO/kernel.t64" ::
MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT32_IMG" "$ISO/recovery.t64" ::

# --- 6. Build ISO ---
echo "[5] Building ISO image ($ISO_IMG)..."
# We use efiboot.img as ESP, similar to original behavior
dd if=/dev/zero of="$EFI_IMG" bs=1M count=4 2>/dev/null
mkfs.vfat "$EFI_IMG" > /dev/null
mcopy -s -i "$EFI_IMG" "$ISO/EFI" ::
mcopy -i "$EFI_IMG" "$ISO/kernel.t64" ::
mcopy -i "$EFI_IMG" "$ISO/recovery.t64" ::
cp "$EFI_IMG" "$ISO/"
xorriso -as mkisofs -V 'TINY64' -e efiboot.img -no-emul-boot -o "$ISO_IMG" "$ISO" 2>/dev/null

echo ""
echo "[6] Build Complete!"
echo "Generated files:"
echo "  $FAT32_IMG (can DD directly to USB/VM disk for UEFI)"
echo "  $ISO_IMG   (bootable ISO for UEFI)"

# --- 7. Auto-run QEMU (ISO only as before, FAT32 is for USB/hardware) ---
echo "Running with: qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -cdrom $ISO_IMG -m 256M -vga std -serial mon:stdio"
echo "Or: qemu-system-x86_64 -bios ./OVMF.fd -cdrom $ISO_IMG -m 256M -vga std"
echo ""
sleep 1

if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    # Prefer system OVMF if available, fallback to local if not
    if [ -f /usr/share/ovmf/OVMF.fd ]; then
        qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -cdrom "$ISO_IMG" -m 256M -vga std -serial mon:stdio
    elif [ -f ./OVMF.fd ]; then
        qemu-system-x86_64 -bios ./OVMF.fd -cdrom "$ISO_IMG" -m 256M -vga std -serial mon:stdio

    else
        echo "Warning: OVMF firmware not found. Please install OVMF or provide OVMF.fd in project root." >&2
        exit 1
    fi
else
    echo "Error: qemu-system-x86_64 not found in PATH. Please install QEMU." >&2
    exit 1
fi