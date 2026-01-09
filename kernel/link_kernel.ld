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
}