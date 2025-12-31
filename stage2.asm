; stage2.asm - 32-bit ELF32 loader (runs at 0x8000)
org 0x8000
bits 32

ELF_BASE equ 0x9000
PT_LOAD  equ 1

start:
    ; Clear screen
    mov edi, 0xB8000
    mov ecx, 80*25
    mov ax, 0x0720
.clear:
    stosw
    loop .clear

    ; Print "S2" at top-left
    mov dword [0xB8000], 0x0F320F53   ; 'S' '2' with attribute 0x0F

    ; Check ELF magic: 0x7F 'E' 'L' 'F'
    cmp dword [ELF_BASE + 0x00], 0x464C457F
    jne elf_fail

    ; Read entry point
    mov ebx, dword [ELF_BASE + 0x18]  ; e_entry

    ; Program header table info
    mov esi, dword [ELF_BASE + 0x1C]  ; e_phoff
    add esi, ELF_BASE                 ; ESI = phdr_ptr

    movzx ecx, word [ELF_BASE + 0x2C] ; e_phnum  (loop counter)
    movzx eax, word [ELF_BASE + 0x2A] ; e_phentsize
    mov [phentsize], eax

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

    mov esi, eax                      ; ESI = src
    mov ecx, ebp                      ; ECX = filesz
    rep movsb                         ; copies bytes, advances EDI

    ; Zero (memsz - filesz)
    sub edx, ebp
    jz .no_bss
    xor eax, eax
    mov ecx, edx
    rep stosb                         ; zeros, advances EDI
.no_bss:

    pop ecx
    pop esi

next_ph:
    ; advance to next program header
    add esi, dword [phentsize]
    dec ecx
    jmp ph_loop

done:
    ; Jump to ELF entry point
    jmp ebx

elf_fail:
    ; Print "EF" on row 1 if ELF parse fails
    mov dword [0xB8000 + 160], 0x0F460F45  ; 'E''F'
.hang:
    hlt
    jmp .hang

phentsize dd 0