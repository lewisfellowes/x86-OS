bits 32
global _start
extern kmain

section .bss
align 16
resb 16384
stack_top:

section .text
_start:
    mov esp, stack_top
    and esp, 0xFFFFFFF0
    push eax
    call kmain
.hang:
    cli
    hlt
    jmp .hang
