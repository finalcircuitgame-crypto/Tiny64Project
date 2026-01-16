# kernel/entry.S
.section .text.entry
.global _start
.extern kernel_main
.extern bios_boot_init

# Entry point for both BIOS and UEFI boot
_start:
    cli                         # Disable interrupts

    # Check if we're running under UEFI or BIOS
    # UEFI puts magic value in RAX, BIOS doesn't
    movq $0xFEDCBA9876543210, %rbx
    cmpq %rbx, %rax
    je uefi_boot

    # BIOS boot path
bios_boot:
    # Set up GDT for BIOS boot (if not already done)
    call bios_boot_init

    # Create minimal BootInfo for BIOS
    pushq $0                    # padding
    pushq $0                    # reserved
    pushq $0                    # pitch (unknown)
    pushq $480                  # height (VGA default)
    pushq $640                  # width (VGA default)
    movq %rsp, %rdi             # BootInfo pointer

    # Set up stack
    movq $0x90000, %rsp
    movq %rsp, %rbp
    andq $-16, %rsp

    call kernel_main
    jmp hang

uefi_boot:
    # UEFI boot path (original code)
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