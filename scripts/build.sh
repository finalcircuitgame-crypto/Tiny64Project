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

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
GRAY='\033[0;37m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Animation characters
SPIN_CHARS="|/-\\"
PROGRESS_CHARS="▏▎▍▌▋▊▉█"
LOADING_CHARS="⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏⠐"
SUCCESS_CHARS="✓✓✓"
ERROR_CHARS="✗✗✗"

# Unicode box drawing characters
BOX_TL='╔'
BOX_TR='╗'
BOX_BL='╚'
BOX_BR='╝'
BOX_H='═'
BOX_V='║'
BOX_CROSS='╬'

# Track build start time
BUILD_START_TIME=$(date +%s)
BUILD_START_DATE=$(date '+%Y-%m-%d %H:%M:%S')

# Animation functions
show_spinner() {
    local pid=$1
    local message=$2
    local spin_idx=0
    local color=${3:-$CYAN}
    while kill -0 $pid 2>/dev/null; do
        printf "\r${color}[${SPIN_CHARS:$spin_idx:1}]${NC} %s" "$message"
        spin_idx=$(( (spin_idx + 1) % 4 ))
        sleep 0.1
    done
    printf "\r${GREEN}[${SUCCESS_CHARS:0:1}]${NC} %s\n" "$message"
}

show_loading() {
    local pid=$1
    local message=$2
    local load_idx=0
    local color=${3:-$YELLOW}
    while kill -0 $pid 2>/dev/null; do
        printf "\r${color}[${LOAD_CHARS:$load_idx:1}]${NC} %s" "$message"
        load_idx=$(( (load_idx + 1) % 8 ))
        sleep 0.15
    done
    printf "\r${GREEN}[${SUCCESS_CHARS:0:1}]${NC} %s\n" "$message"
}

show_progress() {
    local current=$1
    local total=$2
    local message=$3
    local percent=$((current * 100 / total))
    local filled=$((percent / 5))
    local empty=$((20 - filled))
    local bar=""
    
    # Build progress bar
    for ((i=0; i<filled; i++)); do
        bar+="${PROGRESS_CHARS:7:1}"
    done
    for ((i=0; i<empty; i++)); do
        bar+="${PROGRESS_CHARS:0:1}"
    done
    
    printf "\r${CYAN}[%3d%%]${NC} %s [%s]" $percent "$message" "$bar"
}

format_time() {
    local seconds=$1
    local hours=$((seconds / 3600))
    local minutes=$(((seconds % 3600) / 60))
    local secs=$((seconds % 60))
    
    if [ $hours -gt 0 ]; then
        printf "%dh %dm %ds" $hours $minutes $secs
    elif [ $minutes -gt 0 ]; then
        printf "%dm %ds" $minutes $secs
    else
        printf "%ds" $secs
    fi
}

estimate_remaining() {
    local elapsed=$1
    local progress=$2
    if [ $progress -gt 0 ]; then
        local remaining=$((elapsed * (100 - progress) / progress))
        format_time $remaining
    else
        echo "calculating..."
    fi
}

# Enhanced progress display
show_phase_header() {
    local phase=$1
    local icon=$2
    local color=${3:-$BLUE}
    printf "\n${color}${BOLD}=== %s %s ===${NC}\n" "$phase" "$icon"
}

show_phase_result() {
    local phase=$1
    local status=$2
    local color=$3
    if [ "$status" = "SUCCESS" ]; then
        printf "${GREEN}${BOLD}✓ %s completed successfully${NC}\n" "$phase"
    else
        printf "${RED}${BOLD}✗ %s failed${NC}\n" "$phase"
    fi
}

# Build summary with enhanced visuals
show_build_summary() {
    local build_end_time=$(date +%s)
    local build_duration=$((build_end_time - BUILD_START_TIME))
    local kernel_size=$(stat -c %s "$BIN/kernel.elf" 2>/dev/null | awk '{print $1}' || echo "0")
    local iso_size=$(stat -c %s "$ISO_IMG" 2>/dev/null | awk '{print $1}' || echo "0")
    local wad_size=$(stat -c %s "$PROJECT_ROOT/doom.wad" 2>/dev/null | awk '{print $1}' || echo "0")
    
    echo ""
    echo "${PURPLE}${BOX_TL}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_TR}${NC}"
    echo "${PURPLE}${BOX_V}${NC} ${GREEN}${BOLD}✓ TINY64 OS BUILD COMPLETED!${NC} ${PURPLE}${BOX_V}${NC}"
    echo "${PURPLE}${BOX_C}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_C}${BOX_V}${NC}"
    echo "${PURPLE}${BOX_V}${NC} ${CYAN}Build Duration:${NC} $(format_time $build_duration) ${PURPLE}${BOX_V}${NC}"
    echo "${PURPLE}${BOX_V}${NC} ${CYAN}Kernel Size:${NC} $((kernel_size / 1048576))MB $((kernel_size % 1048576 / 1024))KB ${PURPLE}${BOX_V}${NC}"
    echo "${PURPLE}${BOX_V}${NC} ${CYAN}ISO Size:${NC} $((iso_size / 1048576))MB $((iso_size % 1048576 / 1024))KB ${PURPLE}${BOX_V}${NC}"
    echo "${PURPLE}${BOX_V}${NC} ${CYAN}Objects Compiled:${NC} ${#OBJ_FILES[@]} ${PURPLE}${BOX_V}${NC}"
    if [ -f "$PROJECT_ROOT/doom.wad" ]; then
        echo "${PURPLE}${BOX_V}${NC} ${CYAN}Doom WAD:${NC} $((wad_size / 1048576))MB $((wad_size % 1048576 / 1024))KB ${PURPLE}${BOX_V}${NC}"
    fi
    echo "${PURPLE}${BOX_V}${NC} ${YELLOW}Ready to boot: ./run.sh${NC} ${PURPLE}${BOX_V}${NC}"
    echo "${PURPLE}${BOX_BL}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_BR}${NC}"
    echo ""
}
ISO_IMG="Tiny64.iso"
EFI_IMG="efiboot.img"

# Parse command line arguments
CLEAN_BUILD=true
PARALLEL_JOBS=$(nproc 2>/dev/null || echo 4)

while [[ $# -gt 0 ]]; do
    case $1 in
        --incremental)
            CLEAN_BUILD=false
            shift
            ;;
        --jobs|-j)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --incremental    Skip clean build, only rebuild changed files"
            echo "  --jobs, -j N     Use N parallel jobs (default: auto-detect)"
            echo "  --help, -h       Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

if [ "$CLEAN_BUILD" = true ]; then
    echo "Clean build enabled"
    rm -rf "$BIN" "$ISO" "$ISO_IMG" "$EFI_IMG" "$FAT32_IMG"
else
    echo "Incremental build enabled (only rebuilding changed files)"
    rm -rf "$ISO" "$ISO_IMG" "$EFI_IMG" "$FAT32_IMG"
fi

mkdir -p "$BIN"
mkdir -p "$ISO/EFI/BOOT"

# Build header with enhanced animation
echo ""
echo "${PURPLE}${BOX_TL}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_TR}${NC}"
echo "${PURPLE}${BOX_V}${NC} ${CYAN}${BOLD}Tiny64 OS Build System${NC} ${PURPLE}${BOX_V}${NC}"
echo "${PURPLE}${BOX_V}${NC} ${YELLOW}Started: $BUILD_START_DATE${NC} ${PURPLE}${BOX_V}${NC}"
echo "${PURPLE}${BOX_V}${NC} ${GREEN}Mode: $([ "$CLEAN_BUILD" = true ] && echo "Clean Build" || echo "Incremental")${NC} ${PURPLE}${BOX_V}${NC}"
echo "${PURPLE}${BOX_V}${NC} ${BLUE}Jobs: $PARALLEL_JOBS parallel${NC} ${PURPLE}${BOX_V}${NC}"
echo "${PURPLE}${BOX_BL}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_H}${BOX_BR}${NC}"
echo ""

# Track build phases
BUILD_PHASES=6
CURRENT_PHASE=0

update_build_progress() {
    CURRENT_PHASE=$((CURRENT_PHASE + 1))
    local elapsed=$(($(date +%s) - BUILD_START_TIME))
    local progress=$((CURRENT_PHASE * 100 / BUILD_PHASES))
    local eta=$(estimate_remaining $elapsed $progress)
    
    show_progress $CURRENT_PHASE $BUILD_PHASES "Building Tiny64 OS"
    echo " (Elapsed: $(format_time $elapsed), ETA: $eta)"
}

echo "[0] Bootloader"
x86_64-w64-mingw32-gcc -I. -ffreestanding -mno-red-zone -nostdlib -Wall \
    -Wl,--subsystem,10 -e EfiMain \
    -o "$ISO/EFI/BOOT/BOOTX64.EFI" \
    "$SRC_BOOT/boot.c"

update_build_progress

echo "[2] Kernel"
GCC_FLAGS="-I$SRC_INCLUDE -I$SRC_HAL -I$SRC_DRIVERS -ffreestanding -mno-red-zone -fno-stack-protector -fno-pie -msse2 -c"
OBJ_FILES=()
PIDS=()

# Function to compile a file in parallel
compile_parallel() {
    local src="$1"
    local obj="$2"
    local flags="$3"

    if [ "$CLEAN_BUILD" = true ] || [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
        gcc $flags "$src" -o "$obj" &
        PIDS+=($!)
    else
        echo "  Skipping (up to date): $(basename "$src")"
    fi
}

# Function to wait for all parallel jobs
wait_for_jobs() {
    for pid in "${PIDS[@]}"; do
        wait $pid || exit 1
    done
    PIDS=()
}

# Embed WAD file after OBJ_FILES initialization
show_phase_header "WAD Embedding" "📦" "$PURPLE"
if [ -f "$PROJECT_ROOT/doom.wad" ]; then
    echo "${CYAN}Embedding doom.wad using objcopy method...${NC}"
    
    # Use objcopy to embed the WAD file directly with predictable symbols
    (
    objcopy -I binary -O elf64-x86-64 --binary-architecture i386 \
        --rename-section .data=.rodata,alloc,load,readonly,data,contents \
        --redefine-sym _binary_doom_wad_start=_binary_doom_wad_start \
        --redefine-sym _binary_doom_wad_end=_binary_doom_wad_end \
        --redefine-sym _binary_doom_wad_size=_binary_doom_wad_size \
        "$PROJECT_ROOT/doom.wad" "$BIN/doom_wad.o"
    ) &
    show_loading $! "Embedding doom.wad ($(stat -c %s "$PROJECT_ROOT/doom.wad" | awk '{printf "%.1f", $1/1048576}')MB)" "$YELLOW"
    
    if [ -f "$BIN/doom_wad.o" ]; then
        OBJ_FILES+=("$BIN/doom_wad.o")
        echo "${GREEN}✓ WAD file embedded successfully (size: $(stat -c %s "$PROJECT_ROOT/doom.wad" | awk '{printf "%.1f", $1/1048576}')MB)${NC}"
    else
        echo "${RED}✗ ERROR: Failed to embed WAD file${NC}"
    fi
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
compile_parallel "$ENTRY_SRC" "$ENTRY_OBJ" "$GCC_FLAGS"
OBJ_FILES+=("$ENTRY_OBJ")

# Compile core kernel files in parallel
show_phase_header "Kernel Compilation" "⚙️" "$BLUE"
echo "${CYAN}Compiling core kernel files ($PARALLEL_JOBS parallel jobs)...${NC}"
for src in "$SRC_GRAPHICS/graphics.c" "$SRC_GRAPHICS/font.c" "$SRC_FS/memory.c" "$SRC_FS/fs.c" "$SRC_STUBS/string.c" "$SRC_GRAPHICS/ttf.c" "$SRC_GRAPHICS/inter_font_data.c" "$SRC_GRAPHICS/winxp_ui.c" "$SRC_CORE/kernel.c" "$SRC_DOOM/m_argv.c" "$SRC_STUBS/system_stubs.c" "$SRC_STUBS/doom_stubs.c" "$SRC_DOOM/d_main.c" "$SRC_DOOM/doomgeneric_tiny64.c"; do
    obj="$BIN/$(basename "$src" .c).o"
    # Avoid compiling kernel_stubs.c and doomgeneric_stubs.c here
    if [[ "$src" != "$SRC_STUBS/kernel_stubs.c" && "$src" != "$SRC_STUBS/doomgeneric_stubs.c" ]]; then
        compile_parallel "$src" "$obj" "$GCC_FLAGS"
        OBJ_FILES+=("$obj")
    fi
done

wait_for_jobs

# Add kernel_stubs.c and doomgeneric_stubs.c exactly once
for src in "$SRC_STUBS/kernel_stubs.c" "$SRC_STUBS/doomgeneric_stubs.c"; do
    obj="$BIN/$(basename "$src" .c).o"
    compile_parallel "$src" "$obj" "$GCC_FLAGS"
    OBJ_FILES+=("$obj")
done

wait_for_jobs

function add_objs_from_dir_exclude() {
    local dir="$1"
    local filter_entry="$2"
    local file_count=0

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
        compile_parallel "$src" "$obj" "$GCC_FLAGS"
        OBJ_FILES+=("$obj")
        file_count=$((file_count + 1))
    done

    if [ $file_count -gt 0 ]; then
        wait_for_jobs
    fi
}

SERIAL_SRC="$SRC_HAL/serial.c"
SERIAL_OBJ="$BIN/serial.o"
compile_parallel "$SERIAL_SRC" "$SERIAL_OBJ" "$GCC_FLAGS"
OBJ_FILES+=("$SERIAL_OBJ")

# Compile drivers manually (with proper include paths)
DRIVER_GCC_FLAGS="$GCC_FLAGS -I$SRC_STUBS -I$SRC_CORE"
cd "$SRC_DRIVERS"
USB_OBJ="$BIN/usb.o"
compile_parallel "usb.c" "$USB_OBJ" "$DRIVER_GCC_FLAGS"
OBJ_FILES+=("$USB_OBJ")

RTL8139_OBJ="$BIN/rtl8139.o"
compile_parallel "rtl8139.c" "$RTL8139_OBJ" "$DRIVER_GCC_FLAGS"
OBJ_FILES+=("$RTL8139_OBJ")

AC97_OBJ="$BIN/ac97.o"
compile_parallel "ac97.c" "$AC97_OBJ" "$DRIVER_GCC_FLAGS"
OBJ_FILES+=("$AC97_OBJ")

IDE_OBJ="$BIN/ide.o"
compile_parallel "ide.c" "$IDE_OBJ" "$DRIVER_GCC_FLAGS"
OBJ_FILES+=("$IDE_OBJ")
cd "$PROJECT_ROOT"

wait_for_jobs

# Compile top-level drivers (keyboard/mouse) so kernel can link input symbols
KEYBOARD_SRC="$PROJECT_ROOT/drivers/keyboard.c"
KEYBOARD_OBJ="$BIN/keyboard.o"
compile_parallel "$KEYBOARD_SRC" "$KEYBOARD_OBJ" "$GCC_FLAGS"
OBJ_FILES+=("$KEYBOARD_OBJ")

MOUSE_SRC="$PROJECT_ROOT/drivers/mouse.c"
MOUSE_OBJ="$BIN/mouse.o"
compile_parallel "$MOUSE_SRC" "$MOUSE_OBJ" "$GCC_FLAGS"
OBJ_FILES+=("$MOUSE_OBJ")

wait_for_jobs

add_objs_from_dir_exclude "$SRC_HAL" no
add_objs_from_dir_exclude "$SRC_CORE" yes
add_objs_from_dir_exclude "$SRC_GRAPHICS" yes
add_objs_from_dir_exclude "$SRC_FS" yes
add_objs_from_dir_exclude "$SRC_DOOM" yes
add_objs_from_dir_exclude "$SRC_STUBS" yes

show_phase_header "Kernel Linking" "🔗" "$YELLOW"
(
ld -T "$SRC_CORE/link_kernel.ld" -o "$BIN/kernel.elf" "${OBJ_FILES[@]}"
objcopy -O binary "$BIN/kernel.elf" "$ISO/kernel.t64"
) &
show_loading $! "Linking kernel with ${#OBJ_FILES[@]} objects" "$YELLOW"

update_build_progress

show_phase_result "Kernel Linking" "SUCCESS" "$GREEN"

echo "[3] Recovery Kernel"
OBJ_RECOVERY=()
PIDS_RECOVERY=()

# Function to compile recovery files in parallel
compile_recovery_parallel() {
    local src="$1"
    local obj="$2"
    local flags="$3"

    if [ "$CLEAN_BUILD" = true ] || [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
        gcc $flags "$src" -o "$obj" &
        PIDS_RECOVERY+=($!)
    else
        echo "  Skipping (up to date): $(basename "$src")"
    fi
}

# Function to wait for all recovery parallel jobs
wait_for_recovery_jobs() {
    for pid in "${PIDS_RECOVERY[@]}"; do
        wait $pid || exit 1
    done
    PIDS_RECOVERY=()
}

OBJ_RECOVERY+=("$ENTRY_OBJ")

for src in $(find "$SRC_HAL" -maxdepth 1 -type f \( -name "*.c" -o -name "*.S" -o -name "*.s" \) | sort); do
    [[ "$(basename "$src")" == "serial.c" ]] && continue
    obj="$BIN/recovery_$(basename "$src" | sed 's/\.\w\+$/.o/')"
    compile_recovery_parallel "$src" "$obj" "$GCC_FLAGS"
    OBJ_RECOVERY+=("$obj")
done

wait_for_recovery_jobs

RECOVERY_C="$SRC_CORE/recovery.c"
RECOVERY_OBJ="$BIN/recovery.o"
compile_recovery_parallel "$RECOVERY_C" "$RECOVERY_OBJ" "$GCC_FLAGS -DRECOVERY_KERNEL"
OBJ_RECOVERY+=("$RECOVERY_OBJ")

GRAPHICS_OBJ="$BIN/recovery_graphics.o"
compile_recovery_parallel "$SRC_GRAPHICS/graphics.c" "$GRAPHICS_OBJ" "$GCC_FLAGS -DRECOVERY_KERNEL"
OBJ_RECOVERY+=("$GRAPHICS_OBJ")

SERIAL_RECOVERY_OBJ="$BIN/recovery_serial.o"
compile_recovery_parallel "$SRC_HAL/serial.c" "$SERIAL_RECOVERY_OBJ" "$GCC_FLAGS"
OBJ_RECOVERY+=("$SERIAL_RECOVERY_OBJ")

STRING_RECOVERY_OBJ="$BIN/recovery_string.o"
compile_recovery_parallel "$SRC_STUBS/string.c" "$STRING_RECOVERY_OBJ" "$GCC_FLAGS"
OBJ_RECOVERY+=("$STRING_RECOVERY_OBJ")

FONT_OBJ="$BIN/recovery_font.o"
compile_recovery_parallel "$SRC_GRAPHICS/font.c" "$FONT_OBJ" "$GCC_FLAGS"
OBJ_RECOVERY+=("$FONT_OBJ")

wait_for_recovery_jobs

for src in $(find "$SRC_DRIVERS" -maxdepth 1 -type f \( -name "*.c" -o -name "*.S" -o -name "*.s" \) | sort); do
    obj="$BIN/recovery_$(basename "$src" | sed 's/\.\w\+$/.o/')"
    compile_recovery_parallel "$src" "$obj" "$GCC_FLAGS"
    OBJ_RECOVERY+=("$obj")
done

wait_for_recovery_jobs

# Also compile top-level drivers (keyboard, mouse) into recovery build so interrupts and input handlers link
for src in "$PROJECT_ROOT/drivers/keyboard.c" "$PROJECT_ROOT/drivers/mouse.c"; do
    if [ -f "$src" ]; then
        obj="$BIN/recovery_$(basename "$src" | sed 's/\.\w\+$/.o/')"
        compile_recovery_parallel "$src" "$obj" "$GCC_FLAGS"
        OBJ_RECOVERY+=("$obj")
    fi
done

wait_for_recovery_jobs

# Include minimal memory allocation for recovery build (kmalloc/kfree used by drivers)
MEMORY_RECOVERY_OBJ="$BIN/recovery_memory.o"
compile_recovery_parallel "$SRC_FS/memory.c" "$MEMORY_RECOVERY_OBJ" "$GCC_FLAGS"
OBJ_RECOVERY+=("$MEMORY_RECOVERY_OBJ")

wait_for_recovery_jobs

echo "[3.5] Link Recovery"
(
ld -T "$SRC_CORE/link_kernel.ld" -o "$BIN/recovery.elf" "${OBJ_RECOVERY[@]}"
objcopy -O binary "$BIN/recovery.elf" "$ISO/recovery.t64"
) &
show_spinner $! "Linking recovery kernel"

update_build_progress

echo "[4] FAT32 Image"
(
dd if=/dev/zero of="$FAT32_IMG" bs=1M count=16 2>/dev/null
mkfs.vfat "$FAT32_IMG" >/dev/null
MTOOLS_SKIP_CHECK=1 mmd -i "$FAT32_IMG" ::/EFI
MTOOLS_SKIP_CHECK=1 mmd -i "$FAT32_IMG" ::/EFI/BOOT
MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT32_IMG" "$ISO/EFI/BOOT/BOOTX64.EFI" ::/EFI/BOOT/
MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT32_IMG" "$ISO/kernel.t64" ::
MTOOLS_SKIP_CHECK=1 mcopy -i "$FAT32_IMG" "$ISO/recovery.t64" ::
) &
show_spinner $! "Creating FAT32 filesystem"

echo "[5] EFI Image"
(
dd if=/dev/zero of="$EFI_IMG" bs=1M count=64 2>/dev/null
mkfs.vfat "$EFI_IMG" >/dev/null
MTOOLS_SKIP_CHECK=1 mcopy -s -i "$EFI_IMG" "$ISO/EFI" ::
MTOOLS_SKIP_CHECK=1 mcopy -i "$EFI_IMG" "$ISO/kernel.t64" ::
MTOOLS_SKIP_CHECK=1 mcopy -i "$EFI_IMG" "$ISO/recovery.t64" ::
cp "$EFI_IMG" "$ISO/"
) &
show_spinner $! "Creating EFI boot image"

update_build_progress

echo "[6] ISO"
(
xorriso -as mkisofs -V 'TINY64' -e efiboot.img -no-emul-boot -o "$ISO_IMG" "$ISO" 2>/dev/null
) &
show_spinner $! "Creating ISO image"

update_build_progress

# Calculate build statistics
BUILD_END_TIME=$(date +%s)
BUILD_DURATION=$((BUILD_END_TIME - BUILD_START_TIME))
KERNEL_SIZE=$(stat -c %s "$BIN/kernel.elf" 2>/dev/null || echo 0)
ISO_SIZE=$(stat -c %s "$ISO_IMG" 2>/dev/null || echo 0)

# Final build summary
show_build_summary

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
