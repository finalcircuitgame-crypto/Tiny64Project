# Tiny64 Operating System

A minimal 64-bit operating system with UEFI bootloader, featuring PS/2 input, a windowed desktop, Doom engine integration, and several basic hardware drivers.

## Project Structure

```
Tiny64/
├── boot/                # UEFI bootloader
├── kernel/
│   ├── core/            # kernel core & entry (kernel.c, entry.S, recovery kernel)
│   ├── graphics/        # framebuffer, fonts, UI code
│   ├── fs/              # simple filesystem and memory allocator
│   ├── doom/            # Doom engine ports, IWAD helpers and stubs
│   └── stubs/           # libc/kernel stubs used by Doom and kernel
├── hal/                 # Hardware Abstraction Layer (GDT/IDT, PIC, etc.)
├── drivers/             # Device drivers (keyboard, mouse, usb, rtl8139, ac97, ide)
├── include/             # Shared headers (io.h, kernel.h, etc.)
├── scripts/             # Build scripts (build.sh)
├── bin/                 # Build artifacts (generated)
├── iso_root/            # ISO build directory (generated)
├── OVMF/                # UEFI firmware files
└── Tiny64.iso           # Final bootable ISO (generated)
```

## Features

- **UEFI Bootloader**: Loads the kernel from a FAT32 filesystem
- **64-bit Kernel**: Runs in long mode with proper CPU tables (GDT/IDT)
- **PS/2 Input**: Improved keyboard and mouse initialization and handling
- **Graphics & Windowing**: Framebuffer-based rendering, double-buffering, window support and dynamic font scaling
- **Terminal**: Improved terminal with many built-in commands and fixes (clear, cat, echo, help, mkdir, rm, reboot, shutdown, meminfo, cpuinfo, netinfo, usbinfo, play, plus `ls` and cursor/prompt fixes)
- **Doom Integration**: Tiny64 port of Doom Generic with WAD embedding and in-memory IWAD loading
- **Drivers**: Basic drivers included for:
  - USB (UHCI) host controller
  - RTL8139 Ethernet
  - AC97 audio playback
  - IDE/ATA storage
- **WAD Embedding**: Build-time embedding of IWADs (doom1.wad / doom.wad / doom2.wad) into the kernel binary so Doom can read the data from memory

## Building

Run the build script:
```bash
./scripts/build.sh
```

This will:
1. Compile the UEFI bootloader
2. Compile all kernel modules and drivers
3. Link the kernel binary
4. Create a bootable ISO image
5. (Optionally) launch QEMU for testing

### WAD embedding notes
- Place an IWAD file (for example `doom1.wad`, `doom.wad`, or `doom2.wad`) in the project root before building so the build script can embed it. Example Windows path for this repository:
  - `F:\Tiny64\doom1.wad`  (or `doom.wad` / `doom2.wad`)
- The build generates `bin/embedded_wad.c` (via `xxd -i`), compiles it to `bin/embedded_wad.o` and links it into the kernel. If no IWAD is found, Doom will report "No WAD file found" at runtime.

## Hardware Requirements

- x86_64 CPU with UEFI support
- PS/2 keyboard and mouse (or QEMU emulated)
- Graphics output (framebuffer)
- Optional: USB controller, RTL8139-compatible NIC, AC97 audio device, IDE/ATA storage for full feature testing

## Architecture

The system follows a layered architecture:
- **Bootloader**: UEFI application that loads and starts the kernel
- **HAL**: Hardware abstraction layer managing CPU state (GDT, IDT, PIC)
- **Drivers**: Device-specific code (keyboard, mouse, USB, RTL8139, AC97, IDE)
- **Kernel**: Core OS functionality, windowing, terminal and application support (including Doom)

## Development

The project uses:
- GCC for C compilation
- NASM/inline assembly for low-level stubs
- GNU ld for linking
- QEMU for testing
- OVMF for UEFI firmware

Notes for contributors:
- The kernel source was reorganized into `kernel/core`, `kernel/graphics`, `kernel/fs`, `kernel/doom`, and `kernel/stubs`. Update `scripts/build.sh` if you move files.
- Doom integration relies on small file/I-O stubs in `kernel/doom` that read embedded WAD data. If you change the embedding approach, update `doom_stubs.c`.
- If you encounter linker errors about missing symbols (input handlers, kmalloc/kfree), ensure `drivers/keyboard.c`, `drivers/mouse.c` and `kernel/fs/memory.c` are compiled and linked (the build script includes them in both normal and recovery builds).
