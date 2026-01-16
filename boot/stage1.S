/*
 * Tiny64 BIOS Bootloader - Stage 1
 * Loads Stage 2 bootloader and kernel
 */

.code16
.global _start

.section .text

_start:
    /* Stage 1 loaded at 0x7E00 */
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es

    /* Print stage message */
    movw $stage1_msg, %si
    call print_string

    /* Load Stage 2 bootloader (sectors 2-5, ~2KB) */
    movb $0x02, %ah          /* BIOS read sector function */
    movb $0x04, %al          /* Read 4 sectors */
    movb $0x00, %ch          /* Cylinder 0 */
    movb $0x02, %cl          /* Sector 2 */
    movb $0x00, %dh          /* Head 0 */
    movb boot_drive, %dl     /* Boot drive */
    movw $0x8000, %bx        /* Load to 0x8000 */
    int $0x13
    jc disk_error

    /* Jump to Stage 2 */
    ljmp $0x0000, $0x8000

disk_error:
    movw $stage1_error_msg, %si
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

stage1_msg:         .asciz "Stage 1 loaded\r\n"
stage1_error_msg:   .asciz "Stage 1 error!\r\n"

/* Saved from MBR */
boot_drive:         .byte 0
