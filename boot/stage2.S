/*
 * Tiny64 BIOS Bootloader - Stage 2
 * Sets up protected mode and loads kernel
 */

.code16
.global _start

.section .text

_start:
    /* Stage 2 loaded at 0x8000 */
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es

    /* Print stage message */
    movw $stage2_msg, %si
    call print_string

    /* Load kernel from sectors 6+ to 0x100000 (1MB) */

    movw $0x1000, %ax
    movw %ax, %es              /* ES = 0x1000 */
    movw $0x0000, %bx          /* BX = 0x0000 (ES*16 + BX = 0x100000) */

    movb $0x02, %ah            /* BIOS read sector function */
    movb $0x80, %al            /* Read 128 sectors (~64KB) - adjust if kernel grows */
    movb $0x00, %ch            /* Cylinder 0 */
    movb $0x06, %cl            /* Sector 6 */
    movb $0x00, %dh            /* Head 0 */
    movb boot_drive, %dl
    int $0x13
    jc kernel_error

    /* Enable A20 line */
    call enable_a20

    /* Load GDT */
    lgdt gdt_desc

    /* Enter protected mode */
    movl %cr0, %eax
    orl $0x1, %eax
    movl %eax, %cr0

    /* Jump to protected mode */
    ljmp $0x08, $protected_mode

.code32

protected_mode:
    /* Set up segment registers */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movl $0x90000, %esp

    /* Print protected mode message */
    movl $pm_msg, %esi
    call print_string_pm

    /* Jump to kernel at 0x100000 */
    ljmp $0x08, $0x100000

.code16

enable_a20:
    /* Enable A20 line using keyboard controller */
    call wait_kbc
    movb $0xD0, %al
    outb %al, $0x64
    call wait_kbc
    inb $0x60, %al
    pushw %ax
    call wait_kbc
    movb $0xD1, %al
    outb %al, $0x64
    call wait_kbc
    popw %ax
    orb $0x02, %al
    outb %al, $0x60
    call wait_kbc
    ret

wait_kbc:
    inb $0x64, %al
    testb $0x02, %al
    jnz wait_kbc
    ret

kernel_error:
    movw $kernel_error_msg, %si
    call print_string
    hlt

print_string:
    lodsb
    orb %al, %al
    jz print_done
    movb $0x0E, %ah
    int $0x10
    jmp print_string
print_done:
    ret

print_string_pm:
    lodsb
    orb %al, %al
    jz print_done_pm
    movb $0x0E, %ah
    movw $0x0007, %bx
    int $0x10
    jmp print_string_pm
print_done_pm:
    ret

/* GDT */
.align 8
gdt:
    /* Null descriptor */
    .quad 0x0000000000000000

    /* Code segment */
    .quad 0x00CF9A000000FFFF

    /* Data segment */
    .quad 0x00CF92000000FFFF

gdt_desc:
    .word gdt_desc - gdt - 1
    .long gdt

stage2_msg:        .asciz "Stage 2 loaded\r\n"
kernel_error_msg:  .asciz "Kernel load error!\r\n"
pm_msg:            .asciz "Protected mode enabled\r\n"

/* Saved from Stage 1 */
boot_drive:        .byte 0

/* Pad to fill sectors */
.fill 2048 - (. - _start), 1, 0
