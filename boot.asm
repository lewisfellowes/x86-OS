org 0x7C00
bits 16

; -----------------------------
; Disk / loader layout
; -----------------------------
STAGE2_LOAD_ADDR     equ 0x8000
KERNEL_ELF_ADDR      equ 0x9000

STAGE2_SECTORS       equ 1
KERNEL_SECTORS       equ 32

; -----------------------------
; Boot info + memory map layout
; These live in low memory below 0x8000 so they survive
; when we load stage2 at 0x8000 and the ELF blob at 0x9000.
; -----------------------------
BOOT_INFO_ADDR       equ 0x6000        ; physical address (avoid 0x5000 – BIOS can clobber it)
E820_BUFFER_ADDR     equ 0x5100        ; start of E820 entry array
E820_ENTRY_SIZE      equ 20            ; bytes per E820 entry (base,len,type)
E820_MAX_ENTRIES     equ 32            ; hard cap to avoid overruns

; boot_info struct layout (32-bit words unless noted)
;   +0  : magic      (0x1BADB002 for now)
;   +4  : version    (0x00000001)
;   +8  : flags      (reserved, 0 for now)
;   +12 : memmap_ptr (physical pointer to E820 array)
;   +16 : memmap_len (number of valid E820 entries)
;   +20 : boot_drive (BIOS DL as seen by MBR)
;   +21 : _pad[3]    (padding / future fields)
BOOTINFO_MAGIC       equ 0x1BADB002
BOOTINFO_VERSION     equ 0x00000001

start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl
    call enable_a20

    ; Boot info at BOOT_INFO_ADDR – 16-bit writes only (safe in real mode)
    xor ax, ax
    mov ds, ax
    mov bx, BOOT_INFO_ADDR
    mov word [bx + 0], 0xB002          ; magic lo
    mov word [bx + 2], 0x1BAD          ; magic hi
    mov word [bx + 4], 1               ; version
    mov word [bx + 6], 0
    mov word [bx + 8], 0               ; flags
    mov word [bx + 10], 0
    mov word [bx + 12], 0              ; memmap_ptr
    mov word [bx + 14], 0
    mov word [bx + 16], 0              ; memmap_len
    mov word [bx + 18], 0
    mov al, [boot_drive]
    mov [bx + 20], al
    mov word [bx + 22], 0

    ; ---- E820 memory map query ----
    ; INT 15h EAX=E820h enumerates physical memory regions.
    ; Each call fills one 20-byte entry at ES:DI; EBX is the
    ; continuation token (0 = start / last).  EDX must be 'SMAP'.
    xor ebx, ebx
    xor bp, bp
    mov di, E820_BUFFER_ADDR

.e820_loop:
    mov eax, 0x0000E820
    mov cx, E820_ENTRY_SIZE
    mov edx, 0x534D4150       ; 'SMAP'
    int 0x15
    jc .e820_end
    cmp eax, 0x534D4150
    jne .e820_end
    inc bp
    add di, E820_ENTRY_SIZE
    cmp bp, E820_MAX_ENTRIES
    jge .e820_end
    test ebx, ebx
    jnz .e820_loop

.e820_end:
    mov bx, BOOT_INFO_ADDR
    mov word [bx + 12], E820_BUFFER_ADDR
    mov word [bx + 14], 0
    mov [bx + 16], bp
    mov word [bx + 18], 0

    ; Load Stage2 from LBA 1 into 0000:8000
    mov bx, STAGE2_LOAD_ADDR
    mov dword [lba_low], 1
    mov dword [lba_high], 0
    mov di, STAGE2_SECTORS
    call read_sectors

    ; Load Kernel ELF from LBA 2 into 0000:9000
    mov bx, KERNEL_ELF_ADDR
    mov dword [lba_low], 1 + STAGE2_SECTORS
    mov dword [lba_high], 0
    mov di, KERNEL_SECTORS
    call read_sectors

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
; Uses LBA (int13h ext) when available; CHS fallback.
; ---------------------------------------------------------
read_sectors:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

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
    mov word [dap + 2], di          ; blocks to read
    mov bx, [dest_off]
    mov word [dap + 4], bx          ; offset
    mov word [dap + 6], 0x0000      ; segment

    mov eax, [lba_low]
    mov [dap + 8], eax
    mov eax, [lba_high]
    mov [dap + 12], eax

    mov si, dap                     ; DS is 0, so DS:SI is fine
    mov ah, 0x42
    mov dl, [boot_drive]

    mov cx, 3                       ; retry count
.lba_retry:
    int 0x13
    jnc .ok
    dec cx
    jnz .lba_retry
    jmp disk_error

.chs:
    ; ---- CHS fallback with retries ----
    ; Assumes cylinder 0, head 0, sector = LBA + 1
    xor ax, ax
    mov es, ax
    mov bx, [dest_off]

    mov cx, 3                       ; retry count
.chs_retry:
    mov ax, di                      ; AL = sector count
    mov ah, 0x02                    ; BIOS read sectors
    mov ch, 0x00                    ; cylinder
    mov dh, 0x00                    ; head
    mov dl, [boot_drive]
    mov cl, byte [lba_low]
    inc cl                          ; sector number = LBA + 1
    int 0x13
    jnc .ok

    dec cx
    jnz .chs_retry
    jmp disk_error

.ok:
    pop es
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
.hang:
    hlt
    jmp .hang

; -----------------------------
; 32-bit protected mode entry
; -----------------------------
bits 32
pm_entry:
    cld
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Jump to Stage2 (loaded at physical 0x8000)
    jmp CODE_SEL:STAGE2_LOAD_ADDR

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