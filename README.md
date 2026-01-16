# Tiny64 Operating System

A minimal 64-bit operating system with UEFI bootloader, featuring PS/2 mouse and keyboard support.

## Project Structure

```
Tiny64/
├── boot/           # UEFI bootloader
│   ├── boot.c      # Main bootloader code
│   ├── link_boot.ld # Bootloader linker script
│   └── uefi.h      # UEFI definitions
├── kernel/         # Kernel core
│   ├── kernel.c    # Main kernel entry point and UI
│   ├── graphics.c  # Graphics rendering functions
│   ├── font.c      # Inter font rendering
│   ├── fs.c         # Virtual filesystem
│   ├── string.c     # String/memory functions
│   └── link_kernel.ld # Kernel linker script
├── hal/            # Hardware Abstraction Layer
│   ├── gdt.c       # Global Descriptor Table setup
│   ├── gdt.h       # GDT declarations
│   ├── idt.c       # Interrupt Descriptor Table setup
│   └── idt_asm.S   # Assembly interrupt stubs
├── drivers/        # Device drivers
│   ├── keyboard.c  # PS/2 keyboard driver
│   └── mouse.c     # PS/2 mouse driver
├── include/        # Shared headers
│   └── kernel.h    # Main kernel header
├── scripts/        # Build scripts
│   ├── build.sh    # Main build script
│   └── add.sh      # Alternative build script
├── bin/            # Build artifacts (generated)
├── iso_root/       # ISO build directory (generated)
├── OVMF/           # UEFI firmware files
└── Tiny64.iso      # Final bootable ISO (generated)
```

## Features

- **UEFI Bootloader**: Loads the kernel from FAT32 filesystem
- **64-bit Kernel**: Runs in long mode with proper segmentation
- **GDT Setup**: Global Descriptor Table for memory segmentation
- **IDT Setup**: Interrupt Descriptor Table for hardware interrupts
- **PS/2 Keyboard**: Interrupt-driven keyboard input
- **PS/2 Mouse**: Polled mouse input with cursor rendering
- **Graphics**: Framebuffer-based graphics with basic UI
- **Terminal Window**: Simple desktop-like interface
 - **Doom (embedded)**: DoomGeneric port with embedded WAD support (launchable from terminal)
 - **USB (basic UHCI)**: Simple UHCI-based USB host support (enumeration framework)
 - **Network (RTL8139)**: RTL8139 Ethernet driver for wired networking
 - **Audio (AC97)**: AC97 driver with PCM playback framework
 - **Storage (IDE)**: Basic ATA/IDE driver for persistent sector I/O
 - **Improved Terminal**: Auto-scaling font, dynamic rows/columns, more built-in commands

## Building

Run the build script:
```bash
./scripts/build.sh
```

This will:
1. Compile the UEFI bootloader
2. Compile all kernel modules
3. Link the kernel binary
4. Create a bootable ISO image
5. Launch QEMU for testing

## Hardware Requirements

- x86_64 CPU with UEFI support
- PS/2 keyboard and mouse
- Graphics output (framebuffer)
 - Optional: RTL8139-compatible Ethernet card for network testing
 - Optional: AC97-compatible audio device for sound testing
 - Optional: IDE disk image for persistent storage testing

## Architecture

The system follows a layered architecture:
- **Bootloader**: UEFI application that loads and starts the kernel
- **HAL**: Hardware abstraction layer managing CPU state (GDT, IDT)
- **Drivers**: Device-specific code for keyboard and mouse
- **Kernel**: Core OS functionality and user interface

## Terminal

The built-in terminal window provides a command prompt and several built-in commands:
- `help` / `?` — show available commands
- `ls` — list files (one per line)
- `cat <file>` — show file contents
- `write <file> <text>` — create/write a file
- `echo <text>` — print text
- `mkdir <dir>` — (placeholder) create directory
- `rm <file>` — (placeholder) remove file
- `clear` / `cls` — clear terminal
- `doom` — launch embedded Doom (requires embedded WAD)
- `wadtest` — diagnostics for embedded WAD detection
- `meminfo`, `cpuinfo`, `netinfo`, `usbinfo` — hardware/status info
- `play <file>` — (placeholder) play audio (AC97)
- `reboot`, `shutdown` — system control

Terminal features:
- Auto-scales font to fit terminal window and screen resolution
- Recalculates rows/columns on window resize
- Cursor wraps and scrolls correctly with scaled glyphs
- Prompt always appears on a clean new line after output

## Drivers (summary)

- `drivers/usb.*` — Minimal UHCI (USB 1.1) host initialization and enumeration framework. Intended as a lightweight foundation (not full stack).
- `drivers/rtl8139.*` — Basic RTL8139 Ethernet driver with RX/TX and simple buffer management.
- `drivers/ac97.*` — AC97 audio support and PCM playback hooks (framework, not a full audio player).
- `drivers/ide.*` — ATA/IDE driver for sector reads/writes, used to persist filesystem data when an image is attached.

## Doom integration

- DoomGeneric is integrated and can be launched from the terminal using the embedded WAD if present.
- Build system can embed `doom1.wad`, `doom.wad`, or `doom2.wad` into the kernel image for testing.

## Notes & Known Issues

- Many drivers are intentionally minimal and provide basic functionality only (placeholders for complex features).
- Wireless (Intel WiFi) drivers are not included due to firmware/protocol complexity.
- Filesystem currently supports an in-memory FAT-like interface; attaching a disk image plus IDE driver allows persistence.
- Audio playback and some file/dir operations are placeholders and need userland or additional kernel code to be fully functional.

## Contributing

Contributions welcome — driver enhancements, full filesystem persistence, and improved terminal features are good next steps.

## Development

The project uses:
- GCC for C compilation
- NASM for assembly (interrupt stubs)
- GNU ld for linking
- QEMU for testing
- OVMF for UEFI firmware
