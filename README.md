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

## Architecture

The system follows a layered architecture:
- **Bootloader**: UEFI application that loads and starts the kernel
- **HAL**: Hardware abstraction layer managing CPU state (GDT, IDT)
- **Drivers**: Device-specific code for keyboard and mouse
- **Kernel**: Core OS functionality and user interface

## Development

The project uses:
- GCC for C compilation
- NASM for assembly (interrupt stubs)
- GNU ld for linking
- QEMU for testing
- OVMF for UEFI firmware
