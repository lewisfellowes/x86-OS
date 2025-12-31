bits 32
global _start

section .text
_start:
    mov edi, 0xB8000 + (80*2*2)
    mov esi, msg
    mov ah, 0x0F

.print:
    lodsb
    test al, al
    jz .hang
    mov [edi], al
    mov [edi+1], ah
    add edi, 2
    jmp .print

.hang:
    hlt
    jmp .hang

section .rodata
msg db "Kernel running at 1MB (ELF loaded by Stage2)!", 0