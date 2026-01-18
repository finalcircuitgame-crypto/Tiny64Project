OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS {
    . = 0x100000;

    .text : {
        KEEP(*(.text.entry))   /* Force entry.S to the very beginning */
        *(.text .text.*)
    }

    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }

    /DISCARD/ : {
        *(.note.gnu.build-id)
        *(.note.GNU-stack)
    }
    /* mark kernel end for external checks */
    __kernel_end = .;
}
/* Ensure kernel fits within the bootloader-provided 16MB region (0x100000-0x10FFFFF).
   If this ASSERT trips during linking, reduce kernel size or increase bootloader allocation. */
ASSERT(__kernel_end <= 0x1000000, "Kernel too large; must be <= 0x1000000 (16MB).");