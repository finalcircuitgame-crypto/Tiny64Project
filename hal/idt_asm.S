.section .text
.global load_idt
.global isr_stub_keyboard
.global isr_stub_mouse
.global isr_stub_double_fault

load_idt:
    lidt (%rdi)
    ret


isr_stub_double_fault:
    cli
    call handle_double_fault
    iretq

.macro ISR_HANDLER func
    cli
    pushq %rdi
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    call \func
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
    popq %rdi
    sti
    iretq
.endm

isr_stub_keyboard: ISR_HANDLER handle_keyboard_interrupt
isr_stub_mouse:    ISR_HANDLER handle_mouse_interrupt
