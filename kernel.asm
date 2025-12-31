; kernel.asm - 32-bit "kernel" loaded at 0x1000
org 0x1000
bits 32

kernel_start:
    ; Clear screen (80x25) with spaces, attribute 0x07
    mov edi, 0xB8000
    mov ecx, 80*25
    mov ax, 0x0720              ; ' ' + attribute 0x07
.clear:
    stosw
    loop .clear

    ; Print message on row 1 (not row 0), so it doesn't collide with BIOS line
    mov edi, 0xB8000 + (80*1*2) ; row 1, col 0
    mov esi, msg
    mov ah, 0x0F                ; bright white on black

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

msg db "Hello from 32-bit protected mode!", 0
