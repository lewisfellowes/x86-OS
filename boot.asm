org 0x7C00
bits 16

STAGE2_LOAD_ADDR    equ 0x8000
KERNEL_ELF_ADDR     equ 0x9000
KERNEL_LOAD_ADDR    equ 0x1000

STAGE2_SECTORS      equ 4
KERNEL_SECTORS      equ 32

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl
    call enable_a20

    ; --- Load Stage2 from LBA 1 into 0000:8000 ---
    mov bx, STAGE2_LOAD_ADDR
    mov dword [lba_low], 1
    mov dword [lba_high], 0
    mov di, STAGE2_SECTORS
    call read_sectors

    ; --- Load Kernel ELF blob from LBA (1 + STAGE2_SECTORS) into 0000:9000 ---
    mov bx, KERNEL_ELF_ADDR
    mov dword [lba_low], 1 + STAGE2_SECTORS
    mov dword [lba_high], 0
    mov di, KERNEL_SECTORS
    call read_sectors

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

; ---------------------------------------------------------
; read_sectors
; Inputs:
;   BX = destination offset (segment = 0000)
;   DI = number of sectors to read
;   [lba_low/high] = starting LBA
; Uses LBA (int13h ext) when available; CHS fallback for floppy.
; ---------------------------------------------------------
read_sectors:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    mov [dest_off], bx          ; save destination offset

    ; Check INT 13h extensions
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .chs
    cmp bx, 0xAA55
    jne .chs
    test cx, 1
    jz .chs

    ; ---- LBA read (AH=42h) ----
    mov word [dap + 2], di
    mov bx, [dest_off]
    mov word [dap + 4], bx
    mov word [dap + 6], 0x0000

    mov eax, [lba_low]
    mov [dap + 8], eax
    mov eax, [lba_high]
    mov [dap + 12], eax

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    jmp .ok

.chs:
    ; ---- CHS fallback with retries ----
    ; Assumes cylinder 0, head 0, sector = LBA + 1
    mov bx, [dest_off]
    mov ax, 0x0000
    mov es, ax

    mov si, 3                  ; retry count

.retry:
    mov ax, di                 ; AX = DI (AL = count)
    mov ah, 0x02               ; AH = read sectors (set AFTER)
    mov ch, 0x00               ; cylinder
    mov dh, 0x00               ; head
    mov dl, [boot_drive]
    mov cl, byte [lba_low]
    inc cl                     ; sector = LBA + 1
    int 0x13
    jnc .ok

.ok:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

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

    ; Jump to Stage2 (loaded at physical 0x8000)
    jmp STAGE2_LOAD_ADDR

; -----------------------------
; Enable A20 (fast A20 via port 0x92)
; -----------------------------
enable_a20:
    in   al, 0x92
    or   al, 00000010b      ; set A20 enable bit
    and  al, 11111110b      ; clear reset bit (safety)
    out  0x92, al
    ret

; -----------------------------
; GDT (flat)
; -----------------------------
bits 16
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

dest_off dw 0

lba_low  dd 0
lba_high dd 0

; Disk Address Packet (DAP) for int13h AH=42h
dap:
    db 0x10
    db 0x00
    dw 0
    dw 0
    dw 0
    dd 0
    dd 0

times 510 - ($ - $$) db 0
dw 0xAA55