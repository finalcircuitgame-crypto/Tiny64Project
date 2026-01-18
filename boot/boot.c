#include "uefi.h"

typedef struct {
  uint32_t *framebuffer;
  uint32_t *backbuffer;  // Double buffering support
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
} BootInfo;

/* Simple Boot Splash Functions */
void draw_pixel(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, uint32_t x, uint32_t y, uint32_t color) {
  if (x >= gop->Mode->Info->HR || y >= gop->Mode->Info->VR) return;
  uint32_t *fb = (uint32_t *)gop->Mode->FBB;
  uint32_t pitch = gop->Mode->Info->PPSL;
  fb[y * pitch + x] = color;
}

void draw_rect(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
  for (uint32_t py = y; py < y + h; py++) {
    for (uint32_t px = x; px < x + w; px++) {
      draw_pixel(gop, px, py, color);
    }
  }
}

void draw_char_simple(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, char c, uint32_t x, uint32_t y, uint32_t color, uint32_t scale) {
  // Very simple 5x7 font for basic characters
  static const uint8_t font[95][5] = {
    // Space to /
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
    // 0-9
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},
    // A-Z (simplified)
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
  };

  if (c < 32 || c > 126) c = '?';
  unsigned char font_index = (unsigned char)(c - 32);

  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 5; col++) {
      if (font[font_index][col] & (1 << row)) {
        draw_rect(gop, x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

void draw_text(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, const char *text, uint32_t x, uint32_t y, uint32_t color, uint32_t scale) {
  while (*text) {
    draw_char_simple(gop, *text, x, y, color, scale);
    x += 6 * scale;
    text++;
  }
}

void draw_boot_splash(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
  uint32_t width = gop->Mode->Info->HR;
  uint32_t height = gop->Mode->Info->VR;

  // Clear screen with dark blue background
  draw_rect(gop, 0, 0, width, height, 0xFF001122);

  // Draw title bar
  draw_rect(gop, 0, 0, width, 80, 0xFF003366);

  // Draw main title
  uint32_t title_y = 20;
  draw_text(gop, "TINY64 OPERATING SYSTEM", width/2 - 150, title_y, 0xFFFFFFFF, 2);

  // Draw subtitle
  draw_text(gop, "UEFI Bootloader v1.0", width/2 - 80, title_y + 30, 0xFFCCCCCC, 1);

  // Draw border
  uint32_t border_color = 0xFF0066AA;
  draw_rect(gop, 0, 0, width, 3, border_color);           // Top
  draw_rect(gop, 0, height-3, width, 3, border_color);   // Bottom
  draw_rect(gop, 0, 0, 3, height, border_color);         // Left
  draw_rect(gop, width-3, 0, 3, height, border_color);   // Right

  // Draw progress bar background
  uint32_t progress_y = height - 100;
  uint32_t progress_width = width - 200;
  uint32_t progress_x = 100;
  draw_rect(gop, progress_x, progress_y, progress_width, 20, 0xFF444444);

  // Draw initial progress (10%)
  draw_rect(gop, progress_x + 2, progress_y + 2, (progress_width - 4) / 10, 16, 0xFF00AA00);
}

void update_boot_progress(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, const char *message, uint32_t percent) {
  uint32_t width = gop->Mode->Info->HR;
  uint32_t height = gop->Mode->Info->VR;

  // Clear status area
  uint32_t status_y = height - 150;
  draw_rect(gop, 50, status_y, width - 100, 30, 0xFF001122);

  // Draw status message
  draw_text(gop, message, 50, status_y + 5, 0xFFFFFF00, 1);

  // Update progress bar
  uint32_t progress_y = height - 100;
  uint32_t progress_width = width - 200;
  uint32_t progress_x = 100;

  // Clear progress bar
  draw_rect(gop, progress_x + 2, progress_y + 2, progress_width - 4, 16, 0xFF444444);

  // Draw new progress
  uint32_t progress_pixels = ((progress_width - 4) * percent) / 100;
  draw_rect(gop, progress_x + 2, progress_y + 2, progress_pixels, 16, 0xFF00AA00);
}

EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;

/* Helper to read CMOS */
uint8_t read_cmos(uint8_t addr) {
  __asm__ volatile("outb %0, $0x70" : : "a"(addr));
  uint8_t res;
  __asm__ volatile("inb $0x71, %0" : "=a"(res));
  return res;
}

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle,
                          EFI_SYSTEM_TABLE *SystemTable) {
  gST = SystemTable;
  gBS = SystemTable->BootServices;

  /* 1. CHECK PERSISTENT CRASH FLAG (CMOS Index 0x34 is usually free) */
  uint8_t crash_val = read_cmos(0x34);
  uint16_t *kernel_path = (uint16_t *)L"kernel.t64";

  if (crash_val == 0xEE) {
    gST->ConOut->OutputString(gST->ConOut,
                              (uint16_t *)L"!! RECOVERY MODE !!\r\n");
    kernel_path = (uint16_t *)L"recovery.t64";
  } else {
    gST->ConOut->OutputString(gST->ConOut,
                              (uint16_t *)L"Tiny64 Bootloader...\r\n");
  }

  /* 2. Standard UEFI Setup (GOP, FS) */
  EFI_GUID gopGuid = EFI_GOP_GUID;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
  gBS->LocateProtocol(&gopGuid, NULL, (void **)&gop);

  /* Show Boot Splash */
  draw_boot_splash(gop);
  update_boot_progress(gop, "Initializing bootloader...", 15);

  /* Basic filesystem detection */
  EFI_GUID fsGuid = EFI_SFSP_GUID;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
  gBS->LocateProtocol(&fsGuid, NULL, (void **)&fs);
  update_boot_progress(gop, "Filesystem ready...", 25);

  EFI_FILE_PROTOCOL *root;
  fs->OpenVolume(fs, &root);

  /* Load the selected kernel */
  update_boot_progress(gop, "Opening kernel file...", 35);
  EFI_FILE_PROTOCOL *kernelFile;
  if (EFI_ERROR(root->Open(root, &kernelFile, kernel_path, 1, 0))) {
    update_boot_progress(gop, "ERROR: Kernel file missing!", 0);
    gST->ConOut->OutputString(gST->ConOut, (uint16_t *)L"Kernel Missing!\r\n");
    for (;;)
      ;
  }

  /* 3. Allocate and Load at 1MB */
  update_boot_progress(gop, "Allocating memory for kernel...", 50);
  EFI_PHYSICAL_ADDRESS kernelBase = 0x100000;
  UINTN pages = 4096;  // 16MB to accommodate embedded doom.wad file (11.1MB) + kernel code
  gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &kernelBase);
  
  // Check if allocation succeeded
  if (kernelBase == 0) {
    print(gop, L"ERROR: Failed to allocate kernel memory\r\n");
    return EFI_OUT_OF_RESOURCES;
  }

  update_boot_progress(gop, "Loading kernel into memory...", 70);
  UINTN kernelSize = pages * 4096;
  kernelFile->Read(kernelFile, &kernelSize, (void *)kernelBase);
  kernelFile->Close(kernelFile);
  update_boot_progress(gop, "Kernel loaded successfully!", 90);

  /* 4. Prepare and Jump */
  BootInfo info;
  info.framebuffer = (uint32_t *)gop->Mode->FBB;
  info.backbuffer = NULL;  // Will be initialized by kernel
  info.width = gop->Mode->Info->HR;
  info.height = gop->Mode->Info->VR;
  info.pitch = gop->Mode->Info->PPSL;

  UINTN mapKey, memMapSize = 0, descSz;
  uint32_t descVer;
  EFI_MEMORY_DESCRIPTOR *memoryMap = NULL;

  // Get memory map size
  gBS->GetMemoryMap(&memMapSize, NULL, &mapKey, &descSz, &descVer);

  // Allocate buffer for memory map (add padding for safety)
  memMapSize += 4096;
  gBS->AllocatePool(EfiLoaderData, memMapSize, (void **)&memoryMap);

  // Get final memory map
  update_boot_progress(gop, "Preparing to exit boot services...", 95);
  gBS->GetMemoryMap(&memMapSize, memoryMap, &mapKey, &descSz, &descVer);

  // Exit boot services
  update_boot_progress(gop, "Starting Tiny64 Kernel...", 100);
  gBS->ExitBootServices(ImageHandle, mapKey);

  void (*kernel_entry)(BootInfo *) = (void (*)(BootInfo *))kernelBase;
  kernel_entry(&info);

  return 0;
}