/*
 * Tiny64 BIOS Bootloader - Master Boot Record
 * Loads Stage 1 bootloader from sector 1
 */

.code16
.global _start

.section .text

_start:
    /* MBR boot code - loaded at 0x7C00 */
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7C00, %sp

    /* Save boot drive number */
    movb %dl, boot_drive

    /* Print loading message */
    movw $loading_msg, %si
    call print_string

    /* Load Stage 1 bootloader (1 sector) from sector 1 */
    movb $0x02, %ah          /* BIOS read sector function */
    movb $0x01, %al          /* Read 1 sector */
    movb $0x00, %ch          /* Cylinder 0 */
    movb $0x01, %cl          /* Sector 1 */
    movb $0x00, %dh          /* Head 0 */
    movb boot_drive, %dl     /* Boot drive */
    movw $0x7E00, %bx        /* Load to 0x7E00 */
    int $0x13
    jc disk_error

    /* Jump to Stage 1 */
    ljmp $0x0000, $0x7E00

disk_error:
    movw $disk_error_msg, %si
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

loading_msg:    .asciz "Tiny64 BIOS Boot\r\n"
disk_error_msg: .asciz "Disk error!\r\n"

boot_drive:     .byte 0

/* Pad to 510 bytes, then add boot signature */
.fill 510 - (. - _start), 1, 0
.word 0xAA55
