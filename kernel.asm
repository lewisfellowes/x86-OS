; kernel.asm - 32-bit kernel loaded at 0x1000
org 0x1000
bits 32

kernel_start:
    ; Print on row 2
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

msg db "Hello from the kernel (32-bit protected mode)!", 0
