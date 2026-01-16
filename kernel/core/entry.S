# kernel/entry.S
.section .text.entry
.global _start
.extern kernel_main

# This is exactly at 0x100000
_start:
    cli                         # Disable interrupts

    # 1. Set up a valid Stack (16KB)
    leaq stack_top(%rip), %rsp
    movq %rsp, %rbp

    # 2. Fix ABI (Language Barrier)
    # UEFI (Bootloader) puts 'BootInfo*' in RCX
    # GCC (Kernel) expects 'BootInfo*' in RDI
    movq %rcx, %rdi

    # 3. Align stack for safety
    andq $-16, %rsp

    # 4. Jump to the C main function
    call kernel_main

    # 5. Halt if kernel returns
hang:
    hlt
    jmp hang

.section .bss
.align 16
stack_bottom:
    .skip 16384                 # Reserve 16KB
stack_top: