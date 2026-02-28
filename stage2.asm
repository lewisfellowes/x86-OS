; stage2.asm - 32-bit ELF32 loader (runs at 0x8000)
org 0x8000
bits 32

ELF_BASE      equ 0x9000
PT_LOAD       equ 1

CODE_SEL      equ 0x08
DATA_SEL      equ 0x10

; Must match boot.asm / kernel expectations
BOOT_INFO_ADDR equ 0x6000

start:
    cli

    ; Load our own known-good GDT
    lgdt [gdt_desc]

    ; Ensure PE=1
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to reload CS using our GDT
    jmp CODE_SEL:pm_start

pm_start:
    ; Reload segments + stack
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    cld

    ; Clear screen
    mov edi, 0xB8000
    mov ecx, 80*25
    mov ax, 0x0720
.clear:
    stosw
    loop .clear

    ; Print "S2"
    mov edi, 0xB8000
    mov bl, 0x0F
    mov esi, msg_s2
    call vga_print

    ; Check ELF magic
    cmp dword [ELF_BASE + 0x00], 0x464C457F
    jne elf_fail

    ; Read entry point into EBX
    mov ebx, dword [ELF_BASE + 0x18]  ; e_entry

    ; Program header pointer = ELF_BASE + e_phoff
    mov esi, dword [ELF_BASE + 0x1C]
    add esi, ELF_BASE

    movzx ecx, word [ELF_BASE + 0x2C] ; e_phnum
    movzx eax, word [ELF_BASE + 0x2A] ; e_phentsize
    mov [phentsize], eax

    mov dword [printed_first], 0

ph_loop:
    test ecx, ecx
    jz done

    ; if p_type != PT_LOAD, skip
    cmp dword [esi + 0x00], PT_LOAD
    jne next_ph

    ; src = ELF_BASE + p_offset
    mov eax, dword [esi + 0x04]       ; p_offset
    add eax, ELF_BASE                 ; EAX = src

    ; dst = p_paddr
    mov edi, dword [esi + 0x0C]       ; p_paddr

    ; filesz, memsz
    mov ebp, dword [esi + 0x10]       ; p_filesz
    mov edx, dword [esi + 0x14]       ; p_memsz

    ; Copy p_filesz bytes: [src] -> [dst]
    push esi
    push ecx

    mov esi, eax              ; src
    mov ecx, ebp              ; filesz
    rep movsb

    ; Zero (memsz - filesz)
    sub edx, ebp
    jz .no_bss
    xor eax, eax
    mov ecx, edx
    rep stosb
.no_bss:

    pop ecx
    pop esi

next_ph:
    add esi, dword [phentsize]
    dec ecx
    jmp ph_loop

done:
    ; Jump to ELF entry point (far).
    ; ABI: EAX = pointer to boot_info struct in low memory.
    mov eax, BOOT_INFO_ADDR
    push dword CODE_SEL
    push ebx
    retf

elf_fail:
    mov edi, 0xB8000 + (80*1*2)
    mov bl, 0x0C
    mov esi, msg_elf_fail
    call vga_print
.hang:
    hlt
    jmp .hang

; -----------------------
; VGA print (BL=attr)
; IN: EDI=VGA dst, ESI=string
; -----------------------
vga_print:
.print:
    lodsb
    test al, al
    jz .done
    mov [edi], al
    mov [edi + 1], bl
    add edi, 2
    jmp .print
.done:
    ret

; -----------------------
; Print EAX as 8 hex chars (BL=attr)
; -----------------------
vga_print_hex32:
    push eax
    push ecx
    push edx

    mov edx, eax
    mov ecx, 8
.hex_loop:
    mov eax, edx
    shr eax, 28
    and eax, 0xF
    add al, '0'
    cmp al, '9'
    jle .ok
    add al, 7
.ok:
    mov [edi], al
    mov [edi + 1], bl
    add edi, 2
    shl edx, 4
    loop .hex_loop

    pop edx
    pop ecx
    pop eax
    ret

; -----------------------------
; Data
; -----------------------------
phentsize      dd 0
printed_first  dd 0

section .rodata
msg_s2       db "S2",0
msg_elf_fail db "ELF FAIL",0

section .data
align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt
gdt_end: