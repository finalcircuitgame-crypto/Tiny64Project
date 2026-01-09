#!/bin/bash
set -e

echo "--- TINY64 REPAIR & BUILD SCRIPT ---"

# 1. Setup Directories
mkdir -p src/boot src/kernel src/include bin iso_root/EFI/BOOT

# ==============================================================================
# 2. RE-CREATE HEADER FILES
# ==============================================================================
cat > src/include/kernel.h << 'EOF'
#ifndef KERNEL_H
#define KERNEL_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t *framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} BootInfo;

/* I/O Helpers */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}
static inline void io_wait(void) { outb(0x80, 0); }

/* Prototypes */
void init_idt(void);
void mouse_init(void);
void handle_mouse(BootInfo *info);
void fill_rect(BootInfo *info, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fill_circle(BootInfo *info, int cx, int cy, int radius, uint32_t color);
void draw_bitmap(BootInfo *info, uint16_t *bitmap, int x, int y, int scale, uint32_t color);
void draw_cursor(BootInfo *info, int x, int y);
void restore_cursor_bg(BootInfo *info, int x, int y);
void draw_char(BootInfo *info, char c, int x, int y, uint32_t color);
void kprint(BootInfo *info, const char *str, int x, int y, uint32_t color);
void keyboard_handler_main(uint8_t scancode);

/* Globals */
extern uint16_t icon_search[];
extern uint16_t icon_folder[];
extern uint16_t icon_term[];
extern const uint8_t font8x8_basic[96][8];
extern char last_key_pressed;
extern uint8_t mouse_left_pressed;
extern int mouse_x;
extern int mouse_y;

#endif
EOF

# ==============================================================================
# 3. RE-CREATE KERNEL SOURCE FILES
# ==============================================================================

# --- KEYBOARD.C ---
cat > src/kernel/keyboard.c << 'EOF'
#include "../include/kernel.h"
char last_key_pressed = 0;
const char scancode_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', 
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 
    '*', 0, ' '
};
void keyboard_handler_main(uint8_t scancode) {
    if (scancode & 0x80) return; 
    if (scancode < 128 && scancode_map[scancode] != 0) {
        last_key_pressed = scancode_map[scancode];
    }
}
EOF

# --- FONT.C ---
cat > src/kernel/font.c << 'EOF'
#include "../include/kernel.h"
const uint8_t font8x8_basic[96][8] = {
    {0,0,0,0,0,0,0,0}, {0x18,0x3C,0x3C,0x18,0x18,0,0x18,0}, {0x66,0x66,0x22,0,0,0,0,0},
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0}, {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0},
    {0,0xC6,0xCC,0x18,0x30,0x66,0xC6,0}, {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0},
    {0x18,0x18,0x30,0,0,0,0,0}, {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0},
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0}, {0,0x66,0x3C,0xFF,0x3C,0x66,0,0},
    {0,0x18,0x18,0x7E,0x18,0x18,0,0}, {0,0,0,0,0,0x18,0x18,0x30},
    {0,0,0,0x7E,0,0,0,0}, {0,0,0,0,0,0x18,0x18,0}, {0,0x02,0x06,0x0C,0x18,0x30,0x60,0x40},
    {0x3C,0x66,0xC3,0xC3,0xC3,0x66,0x3C,0}, {0x18,0x38,0x18,0x18,0x18,0x18,0x3C,0},
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0}, {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0},
    {0x06,0x0E,0x1E,0x36,0x66,0x7F,0x06,0}, {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0},
    {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0}, {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0},
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0}, {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0},
    {0,0x18,0x18,0,0,0x18,0x18,0}, {0,0x18,0x18,0,0,0x18,0x18,0x30},
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0}, {0,0,0x7E,0,0x7E,0,0,0},
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0}, {0x3C,0x66,0x06,0x0C,0x18,0,0x18,0},
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0}, {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0},
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0}, {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0},
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0}, {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0},
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0}, {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0},
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0}, {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0},
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0}, {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0},
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0}, {0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0},
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0}, {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0},
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0}, {0x7C,0x66,0x66,0x66,0x66,0x3C,0x0E,0},
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0}, {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0},
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0}, {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0},
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0}, {0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0xC6,0},
    {0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0}, {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0},
    {0xFE,0x06,0x0C,0x18,0x30,0x60,0xFE,0}, {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0},
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0}, {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0},
    {0x08,0x1C,0x36,0x63,0,0,0,0}, {0,0,0,0,0,0,0,0xFF}, {0x18,0x0C,0x06,0,0,0,0,0},
    {0,0,0x3C,0x06,0x3E,0x66,0x3E,0}, {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0},
    {0,0,0x3C,0x60,0x60,0x60,0x3C,0}, {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0},
    {0,0,0x3C,0x66,0x7E,0x60,0x3C,0}, {0x1C,0x36,0x30,0x78,0x30,0x30,0x30,0},
    {0,0,0x3E,0x66,0x66,0x3E,0x06,0x3C}, {0x60,0x60,0x76,0x66,0x66,0x66,0x66,0},
    {0x18,0,0x38,0x18,0x18,0x18,0x3C,0}, {0x06,0,0x06,0x06,0x06,0x66,0x66,0x3C},
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0}, {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0},
    {0,0,0xEC,0xFE,0xD6,0xC6,0xC6,0}, {0,0,0xDC,0x66,0x66,0x66,0x66,0},
    {0,0,0x3C,0x66,0x66,0x66,0x3C,0}, {0,0,0xDC,0x66,0x66,0x7C,0x60,0xF0},
    {0,0,0x76,0x66,0x66,0x7C,0x06,0x1E}, {0,0,0xDC,0x66,0x60,0x60,0xF0,0},
    {0,0,0x3C,0x60,0x3C,0x06,0x3C,0}, {0x30,0x30,0x78,0x30,0x30,0x36,0x1C,0},
    {0,0,0x66,0x66,0x66,0x66,0x3E,0}, {0,0,0x66,0x66,0x66,0x3C,0x18,0},
    {0,0,0xC6,0xD6,0xFE,0xEE,0xC6,0}, {0,0,0xC6,0x6C,0x38,0x6C,0xC6,0},
    {0,0,0x66,0x66,0x66,0x3E,0x06,0x3C}, {0,0,0x7E,0x0C,0x18,0x30,0x7E,0},
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0}, {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0},
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0}, {0x76,0xDC,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}
};
EOF

# --- GRAPHICS.C ---
cat > src/kernel/graphics.c << 'EOF'
#include "../include/kernel.h"
uint16_t icon_search[] = { 0x0000, 0x07E0, 0x0810, 0x1008, 0x1008, 0x1008, 0x0810, 0x07E0, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0000, 0x0000, 0x0000 };
uint16_t icon_folder[] = { 0x0000, 0x0000, 0x0380, 0x0440, 0x0440, 0x3FF8, 0x2004, 0x2004, 0x2004, 0x2004, 0x2004, 0x2004, 0x3FF8, 0x0000, 0x0000, 0x0000 };
uint16_t icon_term[]   = { 0x0000, 0x7FFE, 0x4002, 0x4002, 0x4802, 0x5402, 0x5202, 0x4102, 0x4002, 0x4002, 0x4032, 0x4032, 0x7FFE, 0x0000, 0x0000, 0x0000 };
void fill_rect(BootInfo *info, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    uint32_t *fb = info->framebuffer;
    for (uint32_t dy = 0; dy < h; dy++) {
        if (y + dy >= info->height) break;
        uint32_t *row = fb + (y + dy) * info->pitch;
        for (uint32_t dx = 0; dx < w; dx++) {
            if (x + dx >= info->width) break;
            row[x + dx] = color;
        }
    }
}
void fill_circle(BootInfo *info, int cx, int cy, int radius, uint32_t color) {
    uint32_t *fb = info->framebuffer;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < (int)info->width && py >= 0 && py < (int)info->height) {
                    fb[py * info->pitch + px] = color;
                }
            }
        }
    }
}
void draw_bitmap(BootInfo *info, uint16_t *bitmap, int x, int y, int scale, uint32_t color) {
    for (int row = 0; row < 16; row++) {
        uint16_t bitmask = bitmap[row];
        for (int col = 0; col < 16; col++) {
            if ((bitmask >> (15 - col)) & 1) {
                fill_rect(info, x + col*scale, y + row*scale, scale, scale, color);
            }
        }
    }
}
void draw_char(BootInfo *info, char c, int x, int y, uint32_t color) {
    if (c < 32 || c > 126) return;
    const uint8_t *glyph = font8x8_basic[c - 32];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if ((glyph[row] >> (7 - col)) & 1) {
                uint32_t index = (y + row) * info->pitch + (x + col);
                if (index < info->width * info->height) info->framebuffer[index] = color;
            }
        }
    }
}
void kprint(BootInfo *info, const char *str, int x, int y, uint32_t color) {
    int cx = x;
    while (*str) {
        draw_char(info, *str, cx, y, color);
        cx += 9;
        str++;
    }
}
EOF

# ==============================================================================
# 4. FIX BOOTLOADER (Error Checking & Fixed Address)
# ==============================================================================
cat > src/boot/boot.c << 'EOF'
#include "uefi.h"
typedef struct {
    uint32_t *framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} BootInfo;

EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gST->ConOut->OutputString(gST->ConOut, (uint16_t*)L"Tiny64 Bootloader...\r\n");

    /* Graphics */
    EFI_GUID gopGuid = EFI_GOP_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    if (EFI_ERROR(gBS->LocateProtocol(&gopGuid, NULL, (void**)&gop))) return 1;

    /* File System */
    EFI_GUID fsGuid = EFI_SFSP_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    if (EFI_ERROR(gBS->LocateProtocol(&fsGuid, NULL, (void**)&fs))) return 1;

    EFI_FILE_PROTOCOL *root, *kernelFile;
    fs->OpenVolume(fs, &root);
    if (EFI_ERROR(root->Open(root, &kernelFile, (uint16_t*)L"kernel.t64", 1, 0))) {
        gST->ConOut->OutputString(gST->ConOut, (uint16_t*)L"ERROR: kernel.t64 not found!\r\n");
        for(;;) __asm__("hlt"); // Halt so user sees error
    }

    /* Fixed Address: 0x100000 (1MB) */
    EFI_PHYSICAL_ADDRESS kernelBase = 0x100000;
    UINTN pages = 512; 
    if (EFI_ERROR(gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &kernelBase))) {
        gST->ConOut->OutputString(gST->ConOut, (uint16_t*)L"ERROR: Alloc 1MB failed.\r\n");
        for(;;) __asm__("hlt");
    }

    /* Read File */
    void *kernelBuffer = (void*)kernelBase;
    UINTN kernelSize = pages * 4096;
    EFI_STATUS readStatus = kernelFile->Read(kernelFile, &kernelSize, kernelBuffer);
    kernelFile->Close(kernelFile);

    if (EFI_ERROR(readStatus) || kernelSize == 0) {
        gST->ConOut->OutputString(gST->ConOut, (uint16_t*)L"ERROR: Read Failed or Empty File.\r\n");
        for(;;) __asm__("hlt");
    }

    gST->ConOut->OutputString(gST->ConOut, (uint16_t*)L"Kernel Loaded. Launching...\r\n");

    BootInfo info;
    info.framebuffer = (uint32_t*)gop->Mode->FBB;
    info.width = gop->Mode->Info->HR;
    info.height = gop->Mode->Info->VR;
    info.pitch = gop->Mode->Info->PPSL;

    /* Exit Boot Services */
    UINTN mapKey, memMapSize = 0, descSz;
    uint32_t descVer;
    gBS->GetMemoryMap(&memMapSize, NULL, &mapKey, &descSz, &descVer);
    memMapSize += 4096;
    gBS->AllocatePool(EfiLoaderData, memMapSize, (void**)&gST);
    gBS->GetMemoryMap(&memMapSize, (EFI_MEMORY_DESCRIPTOR*)gST, &mapKey, &descSz, &descVer);
    gBS->ExitBootServices(ImageHandle, mapKey);

    void (*kernel_entry)(BootInfo*) = (void (*)(BootInfo*))kernelBase;
    kernel_entry(&info);
    return 0;
}
EOF

# ==============================================================================
# 5. LINKER SCRIPT (FORCE 1MB)
# ==============================================================================
cat > src/kernel/link_kernel.ld << 'EOF'
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)
SECTIONS {
    . = 0x100000; /* Must match boot.c kernelBase */
    .text : { *(.text) *(.text.*) }
    .rodata : { *(.rodata) *(.rodata.*) }
    .data : { *(.data) *(.data.*) }
    .bss : { *(.bss) *(.bss.*) *(COMMON) }
}
EOF

# ==============================================================================
# 6. COMPILE AND RUN
# ==============================================================================
echo "[1] Compiling Bootloader..."
x86_64-w64-mingw32-gcc -ffreestanding -mno-red-zone -nostdlib -Wall -Wl,--subsystem,10 -e EfiMain -o iso_root/EFI/BOOT/BOOTX64.EFI src/boot/boot.c

echo "[2] Compiling Kernel..."
GCC_FLAGS="-ffreestanding -mno-red-zone -fno-stack-protector -fno-pie -mgeneral-regs-only -c"

# Clean bin
rm -f bin/*.o

gcc $GCC_FLAGS src/kernel/kernel.c -o bin/kernel.o
gcc $GCC_FLAGS src/kernel/idt.c -o bin/idt.o
gcc $GCC_FLAGS src/kernel/idt_asm.S -o bin/idt_asm.o
gcc $GCC_FLAGS src/kernel/graphics.c -o bin/graphics.o
gcc $GCC_FLAGS src/kernel/mouse.c -o bin/mouse.o
gcc $GCC_FLAGS src/kernel/font.c -o bin/font.o
gcc $GCC_FLAGS src/kernel/keyboard.c -o bin/keyboard.o

echo "[3] Linking Kernel..."
# LINKING ORDER MATTERS: kernel.o must be first!
ld -T src/kernel/link_kernel.ld -o bin/kernel.elf \
    bin/kernel.o \
    bin/idt_asm.o \
    bin/idt.o \
    bin/graphics.o \
    bin/mouse.o \
    bin/font.o \
    bin/keyboard.o

# Check if kernel.elf exists
if [ ! -f bin/kernel.elf ]; then
    echo "ERROR: Link failed."
    exit 1
fi

objcopy -O binary bin/kernel.elf iso_root/kernel.t64

echo "[4] Building ISO..."
rm -f efiboot.img Tiny64.iso
dd if=/dev/zero of=efiboot.img bs=1M count=4 2>/dev/null
mkfs.vfat efiboot.img > /dev/null
mcopy -s -i efiboot.img iso_root/EFI ::
mcopy -i efiboot.img iso_root/kernel.t64 ::
cp efiboot.img iso_root/
xorriso -as mkisofs -V 'TINY64' -e efiboot.img -no-emul-boot -o Tiny64.iso iso_root 2>/dev/null

echo "[5] Running QEMU..."
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -cdrom Tiny64.iso -m 256M -vga std