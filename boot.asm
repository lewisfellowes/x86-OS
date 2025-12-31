org 0x7C00
bits 16

KERNEL_LOAD_ADDR equ 0x1000
KERNEL_SECTORS   equ 4

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; Try INT 13h Extensions first (works for HDD)
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .chs_fallback
    cmp bx, 0xAA55
    jne .chs_fallback
    test cx, 1                  ; bit0 = ext disk access functions (42h) supported
    jz .chs_fallback

    ; ---- LBA read (AH=42h) ----
    mov word [dap + 2], KERNEL_SECTORS
    mov word [dap + 4], KERNEL_LOAD_ADDR
    mov word [dap + 6], 0x0000
    mov dword [dap + 8], 1      ; LBA 1 = sector after boot sector
    mov dword [dap + 12], 0

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    jmp short loaded_ok

.chs_fallback:
    ; ---- CHS read (AH=02h) ----
    ; Read KERNEL_SECTORS starting at CHS 0/0/2 into 0000:1000
    mov bx, KERNEL_LOAD_ADDR
    mov ah, 0x02
    mov al, KERNEL_SECTORS
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

loaded_ok:
    ; Print '$' (real mode)
    mov ah, 0x0E
    mov al, '$'
    xor bh, bh
    int 0x10

    ; Enter protected mode
    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEL:pm_entry

disk_error:
    mov ah, 0x0E
    mov al, '!'
    xor bh, bh
    int 0x10
    hlt

; -----------------------------
; 32-bit protected mode entry
; -----------------------------
bits 32
pm_entry:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    jmp KERNEL_LOAD_ADDR

; -----------------------------
; GDT
; -----------------------------
align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF

gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt
gdt_end:

CODE_SEL equ 0x08
DATA_SEL equ 0x10

boot_drive db 0

; Disk Address Packet (DAP)
dap:
    db 0x10
    db 0x00
    dw 0
    dw 0
    dw 0
    dq 0

times 510 - ($ - $$) db 0
dw 0xAA55