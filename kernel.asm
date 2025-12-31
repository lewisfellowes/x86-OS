; kernel.asm - 32-bit ELF kernel with IDT + basic exception handlers
bits 32
global _start

IDT_ENTRIES equ 256
IDT_SIZE equ (IDT_ENTRIES * 8)

section .text
_start:
    ; Ensure flat segments (data selector = 0x10 from your stage2 GDT)
    cld
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Print a boot message (row 2)
    mov edi, 0xB8000 + (80*2*2)
    mov esi, msg_boot
    mov ah, 0x0F
    call vga_print

    ; Init IDT and load it
    call idt_init

    ; Print confirmation (row 3)
    mov edi, 0xB8000 + (80*3*2)
    mov esi, msg_idt_loaded
    mov ah, 0x0F
    call vga_print

    ; Trigger an invalid opcode exception to prove the IDT works
    ud2

.hang:
    hlt
    jmp .hang

; -----------------------
; IDT setup
; -----------------------
idt_init:
    ; Set gates for exception vectors we care about:
    ; #UD  (6)  - invalid opcode (no error code)
    ; #GP  (13) - general protection fault (has error code)
    ; #PF  (14) - page fault (has error code)

    push dword isr6
    push dword 6
    call idt_set_gate
    add esp, 8

    push dword isr13
    push dword 13
    call idt_set_gate
    add esp, 8

    push dword isr14
    push dword 14
    call idt_set_gate
    add esp, 8

    ; Load IDTR
    lidt [idtr]
    ret

; void idt_set_gate(int vec, void* handler)
; stack: [esp+4] = vec, [esp+8] = handler
idt_set_gate:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]      ; vec
    mov edx, [ebp + 12]     ; handler addr

    ; entry = idt + vec*8
    lea ecx, [idt]
    shl eax, 3
    add ecx, eax

    ; offset low
    mov word [ecx + 0], dx

    ; selector
    mov word [ecx + 2], 0x08

    ; zero
    mov byte [ecx + 4], 0

    ; type/attr: present=1, DPL=0, type=0xE (32-bit interrupt gate)
    mov byte [ecx + 5], 0x8E

    ; offset high
    shr edx, 16
    mov word [ecx + 6], dx

    pop ebp
    ret

; -----------------------
; ISR stubs
; Normalize stack so common handler sees:
;   [vector, error_code, eip, cs, eflags]
; -----------------------

; #UD - no error code
isr6:
    push dword 0        ; error code (fake)
    push dword 6        ; vector
    jmp isr_common

; #GP - has error code already pushed by CPU
isr13:
    push dword 13       ; vector
    jmp isr_common

; #PF - has error code already pushed by CPU
isr14:
    push dword 14       ; vector
    jmp isr_common

isr_common:
    cli
    cld

    ; Row 8: CR0
    mov edi, 0xB8000 + (80*8*2)
    mov ah, 0x0E
    mov esi, msg_cr0
    call vga_print
    mov bl, 0x0E
    mov eax, cr0
    call vga_print_hex32

    ; Row 9: CS selector (as 32-bit)
    mov edi, 0xB8000 + (80*9*2)
    mov ah, 0x0E
    mov esi, msg_cs
    call vga_print
    mov bl, 0x0E
    xor eax, eax
    mov ax, cs
    call vga_print_hex32

    ; Row 10: GDTR base/limit
    sgdt [gdtr_tmp]
    mov edi, 0xB8000 + (80*10*2)
    mov ah, 0x0E
    mov esi, msg_gdt
    call vga_print
    mov bl, 0x0E
    mov eax, [gdtr_tmp + 2]     ; base
    call vga_print_hex32
    mov esi, msg_lim
    call vga_print
    xor eax, eax
    mov ax, [gdtr_tmp + 0]      ; limit
    call vga_print_hex32

    ; Row 11: IDTR base/limit
    sidt [idtr_tmp]
    mov edi, 0xB8000 + (80*11*2)
    mov ah, 0x0E
    mov esi, msg_idt
    call vga_print
    mov bl, 0x0E
    mov eax, [idtr_tmp + 2]     ; base
    call vga_print_hex32
    mov esi, msg_lim
    call vga_print
    xor eax, eax
    mov ax, [idtr_tmp + 0]      ; limit
    call vga_print_hex32

.halt:
    hlt
    jmp .halt

; -----------------------
; Exception display
; eax = vector, ebx = error code (currently unused here)
; -----------------------
show_exception:
    ; Print header on row 6
    mov edi, 0xB8000 + (80*6*2)
    mov ah, 0x0E

    ; "V=0x" then the vector
    push eax
    mov esi, msg_vec
    call vga_print
    pop eax
    call vga_print_hex32

    ; Print friendly name on row 7
    mov edi, 0xB8000 + (80*7*2)
    mov ah, 0x0E

    cmp eax, 6
    je .ud
    cmp eax, 13
    je .gp
    cmp eax, 14
    je .pf

    mov esi, msg_unknown
    call vga_print
    ret

.ud:
    mov esi, msg_ud
    call vga_print
    ret

.gp:
    mov esi, msg_gp
    call vga_print
    ret

.pf:
    mov esi, msg_pf
    call vga_print
    ret


; -----------------------
; VGA print routine
; IN:  EDI = destination in VGA memory
;      ESI = zero-terminated string
;      AH  = attribute byte
; OUT: EDI advanced
; -----------------------
vga_print:
.print:
    lodsb
    test al, al
    jz .done
    mov [edi], al
    mov [edi + 1], ah
    add edi, 2
    jmp .print
.done:
    ret


; -----------------------
; Print a 32-bit value in EAX as 8 hex chars at [EDI]
; IN:  EAX = value, EDI = destination, BL = attribute
; OUT: EDI advanced by 16 bytes (8 chars * 2)
; -----------------------
vga_print_hex32:
    push eax
    push ecx
    push edx

    mov ecx, 8
.hex_loop:
    mov edx, eax
    shr edx, 28
    and edx, 0xF
    add dl, '0'
    cmp dl, '9'
    jle .ok
    add dl, 7
.ok:
    mov [edi], dl
    mov [edi + 1], bl
    add edi, 2
    shl eax, 4
    loop .hex_loop

    pop edx
    pop ecx
    pop eax
    ret

; -----------------------
; Dump 4 dwords from a referenced "original stack" (legacy helper)
; WARNING: uses offsets assuming caller pushed 6 regs (24 bytes)
; -----------------------
dump_stack4:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi

    mov bl, 0x0E

    mov edi, 0xB8000 + (80*8*2)
    mov ah, 0x0E

    mov esi, s0
    call vga_print
    mov eax, [esp + 24 + 4]
    call vga_print_hex32

    mov esi, s1
    call vga_print
    mov eax, [esp + 24 + 8]
    call vga_print_hex32

    mov esi, s2
    call vga_print
    mov eax, [esp + 24 + 12]
    call vga_print_hex32

    mov esi, s3
    call vga_print
    mov eax, [esp + 24 + 16]
    call vga_print_hex32

    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret


; -----------------------
; Dump 5 dwords from the stack AS IT WAS at call entry.
; Prints:
;   S0=[entry_esp+0] ... S4=[entry_esp+16]
;
; Because this function pushes 2 regs (8 bytes),
; we read using [esp+8+offset].
; -----------------------
dump_stack5:
    push eax
    push esi

    mov bl, 0x0E

    mov esi, s0
    call vga_print
    mov eax, [esp + 8 + 0]
    call vga_print_hex32

    mov esi, s1
    call vga_print
    mov eax, [esp + 8 + 4]
    call vga_print_hex32

    mov esi, s2
    call vga_print
    mov eax, [esp + 8 + 8]
    call vga_print_hex32

    mov esi, s3
    call vga_print
    mov eax, [esp + 8 + 12]
    call vga_print_hex32

    mov esi, s4
    call vga_print
    mov eax, [esp + 8 + 16]
    call vga_print_hex32

    pop esi
    pop eax
    ret

; Dump 5 dwords from exception frame base in EBX:
;  S0=[EBX+0] S1=[EBX+4] S2=[EBX+8] S3=[EBX+12] S4=[EBX+16]
dump_stack5_ptr_ebx:
    push eax
    push ebx
    push esi

    mov bl, 0x0E

    mov esi, s0
    call vga_print
    mov eax, [ebx + 0]
    call vga_print_hex32

    mov esi, s1
    call vga_print
    mov eax, [ebx + 4]
    call vga_print_hex32

    mov esi, s2
    call vga_print
    mov eax, [ebx + 8]
    call vga_print_hex32

    mov esi, s3
    call vga_print
    mov eax, [ebx + 12]
    call vga_print_hex32

    mov esi, s4
    call vga_print
    mov eax, [ebx + 16]
    call vga_print_hex32

    pop esi
    pop ebx
    pop eax
    ret

section .rodata
dbg_s0      db " S0=",0
dbg_s1      db " S1=",0
s0          db " S0=",0
s1          db " S1=",0
s2          db " S2=",0
s3          db " S3=",0
s4          db " S4=",0

msg_cr0 db "CR0=0x",0
msg_gdt db "GDTR.base=0x",0
msg_idt db "IDTR.base=0x",0
msg_lim db " lim=0x",0

msg_vec     db "V=0x", 0
msg_cs      db "CS=0x", 0

msg_boot    db "Kernel @ 1MB: boot OK", 0
msg_idt_loaded     db "IDT loaded. Triggering UD2...", 0
msg_exc     db "EXCEPTION CAUGHT:", 0
msg_ud      db "#UD Invalid Opcode (vector 6)", 0
msg_gp      db "#GP General Protection Fault (vector 13)", 0
msg_pf      db "#PF Page Fault (vector 14)", 0
msg_unknown db "Unknown exception", 0

section .data
gdtr_tmp: times 6 db 0
idtr_tmp: times 6 db 0

align 8
idt:
    times IDT_SIZE db 0

idtr:
    dw IDT_SIZE - 1
    dd idt