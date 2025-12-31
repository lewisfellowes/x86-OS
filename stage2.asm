; stage2.asm - 32-bit stage2 loader stub (runs at 0x8000)
org 0x8000
bits 32

STAGE2_START:
    ; Clear screen
    mov edi, 0xB8000
    mov ecx, 80*25
    mov ax, 0x0720
.clear:
    stosw
    loop .clear

    ; Print "Stage2 OK" on row 1
    mov edi, 0xB8000 + (80*1*2)
    mov esi, msg
    mov ah, 0x0F

.print:
    lodsb
    test al, al
    jz .done
    mov [edi], al
    mov [edi+1], ah
    add edi, 2
    jmp .print

.done:
    ; Jump to kernel (already loaded by stage1 at 0x1000)
    jmp 0x1000

msg db "Stage2 OK (32-bit). Jumping to kernel...", 0
