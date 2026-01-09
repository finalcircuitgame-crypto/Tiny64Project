# src/hal/gdt_asm.S
.section .data
.align 16

# 1. Define the GDT Table
gdt_start:
    .quad 0x0000000000000000    # 0x00: Null Descriptor
gdt_code:
    .quad 0x00af9a000000ffff    # 0x08: Kernel Code (64-bit, Ring 0)
gdt_data:
    .quad 0x00af92000000ffff    # 0x10: Kernel Data (64-bit, Ring 0)
gdt_end:

# 2. Define the GDT Pointer (for lgdt)
.global gdt_ptr
gdt_ptr:
    .word (gdt_end - gdt_start - 1)
    .quad gdt_start

.section .text
.global gdt_flush

gdt_flush:
    # Load the GDT
    leaq gdt_ptr(%rip), %rax
    lgdt (%rax)

    # In 64-bit long mode, we only need to reload data segments
    # CS is already set correctly by the bootloader
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    ret