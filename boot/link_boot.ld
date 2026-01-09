ENTRY(EfiMain)
SECTIONS {
    . = 0;
    .text : ALIGN(4096) { *(.text) *(.text.*) }
    .rodata : ALIGN(4096) { *(.rodata) *(.rodata.*) }
    .data : ALIGN(4096) { *(.data) *(.data.*) }
    .bss : ALIGN(4096) { *(.bss) *(.bss.*) *(COMMON) }
    .reloc : ALIGN(4096) { *(.reloc) }
    /DISCARD/ : { *(.note.gnu.*) *(.eh_frame) }
}
