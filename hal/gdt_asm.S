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
gdt_tss:
    # TSS Descriptor (128 bits for x86_64)
    # Low 64 bits
    .word (tss_end - tss - 1)    # Limit[15:0] (TSS size - 1)
    .word 0                      # Base[15:0] (will be set at runtime)
    .byte 0                      # Base[23:16]
    .byte 0x89                   # Type=1001 (TSS), S=0, DPL=00, P=1
    .byte 0x00                   # Limit[19:16], AVL=0, L=0, D/B=0, G=0
    .byte 0                      # Base[31:24]
    # High 64 bits
    .quad 0                      # Base[63:32], Reserved
gdt_end:

# 2. Define the TSS Structure (104 bytes for x86_64)
.global tss
tss:
    .long 0                          # Reserved
    .quad tss_ist1_stack + 4096      # RSP0 (privilege stack)
    .quad 0                          # RSP1
    .quad 0                          # RSP2
    .quad 0                          # Reserved
    .quad 0                          # IST1 (Interrupt Stack Table 1)
    .quad 0                          # IST2
    .quad 0                          # IST3
    .quad 0                          # IST4
    .quad 0                          # IST5
    .quad 0                          # IST6
    .quad 0                          # IST7
    .quad 0                          # Reserved
    .word 0                          # Reserved
    .word 0                          # I/O Map Base Address
tss_end:

# 3. IST Stack for double-fault handler (4KB, page-aligned)
.align 4096
tss_ist1_stack:
    .skip 4096

# 4. Define the GDT Pointer (for lgdt)
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

.global tss_flush
tss_flush:
    # Set TSS base address in the TSS descriptor
    leaq tss(%rip), %rdx        # Get TSS address
    mov %edx, %eax              # Base[15:0]
    mov %ax, gdt_tss+2(%rip)
    shr $16, %edx
    mov %dl, gdt_tss+4(%rip)    # Base[23:16]
    mov %dh, gdt_tss+7(%rip)    # Base[31:24]
    shr $16, %rdx
    mov %edx, gdt_tss+8(%rip)   # Base[63:32]

    # Load the TSS
    mov $0x18, %ax              # TSS selector (0x18)
    ltr %ax

    ret