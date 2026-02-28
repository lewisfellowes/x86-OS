; kernel.asm - 32-bit ELF kernel with IDT + basic exception handlers
bits 32
global _start

IDT_ENTRIES       equ 256
IDT_SIZE          equ (IDT_ENTRIES * 8)

COM1_PORT         equ 0x3F8

PIC1_CMD          equ 0x20
PIC1_DATA         equ 0x21
PIC2_CMD          equ 0xA0
PIC2_DATA         equ 0xA1

PIT_CHANNEL0      equ 0x40
PIT_COMMAND       equ 0x43

; Must match boot.asm / stage2.asm
BOOT_INFO_ADDR    equ 0x6000
BOOTINFO_MAGIC    equ 0x1BADB002
E820_ENTRY_SIZE   equ 20

section .bss
align 4
boot_info_ptr     resd 1
tick_count        resd 1
last_scancode     resb 1

section .text
_start:
    ; On entry, EAX contains pointer to boot_info struct
    ; prepared by the real-mode bootloader.

    ; Save boot_info pointer for later subsystems.
    mov [boot_info_ptr], eax

    ; Ensure flat segments (data selector = 0x10 from your stage2 GDT)
    cld
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Initialize serial (COM1) for early logging
    call serial_init

    mov esi, serial_msg_hello
    call serial_write_str

    ; Print a boot message (row 2)
    mov edi, 0xB8000 + (80*2*2)
    mov esi, msg_boot
    mov bl, 0x0F
    call vga_print

    ; Print basic boot_info summary (row 4/5)
    call boot_info_print_summary

    ; Print E820 memory map to serial + usable RAM total to VGA
    call print_memory_map

    ; Init IDT and load it
    call idt_init

    ; Remap PIC: IRQ0-7 -> vectors 0x20-0x27, IRQ8-15 -> 0x28-0x2F
    call pic_remap

    ; Program PIT channel 0 for ~100 Hz periodic interrupts
    call pit_init_100hz

    ; Print confirmation (row 3)
    mov edi, 0xB8000 + (80*3*2)
    mov esi, msg_idt_loaded
    mov bl, 0x0F
    call vga_print

    mov esi, serial_msg_irq_on
    call serial_write_str

    ; Enable hardware interrupts and enter idle loop
    sti
.halt_loop:
    hlt
    jmp .halt_loop


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

    ; Hardware IRQs (after PIC remap to 0x20–0x2F)
    ; IRQ0 – PIT timer at vector 32
    push dword irq0
    push dword 32
    call idt_set_gate
    add esp, 8

    ; IRQ1 – keyboard at vector 33
    push dword irq1
    push dword 33
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
; ISR stubs (standard layout)
; After stub pushes, stack is:
;   [vector] [error] [eip] [cs] [eflags]
; -----------------------

; #UD - no CPU error code
isr6:
    push dword 0          ; error
    push dword 6          ; vector
    jmp isr_common

; #GP - CPU pushes error code already
; CPU: [err][eip][cs][eflags]
isr13:
    push dword 13         ; vector
    jmp isr_common

; #PF - CPU pushes error code already
isr14:
    push dword 14         ; vector
    jmp isr_common

; IRQ0 – PIT timer
irq0:
    cli
    pushad

    ; Increment global tick counter
    inc dword [tick_count]

    ; Display tick count on VGA row 8
    mov edi, 0xB8000 + (80*8*2)
    mov bl, 0x0A
    mov esi, msg_ticks
    call vga_print
    mov eax, [tick_count]
    call vga_print_hex32

    ; Send End Of Interrupt to master PIC
    mov al, 0x20
    out PIC1_CMD, al

    popad
    sti
    iretd

; IRQ1 – keyboard
irq1:
    cli
    pushad

    ; Read scancode from keyboard controller
    in  al, 0x60
    mov [last_scancode], al

    ; Display last scancode on VGA row 9
    mov edi, 0xB8000 + (80*9*2)
    mov bl, 0x0A
    mov esi, msg_key
    call vga_print
    movzx eax, byte [last_scancode]
    call vga_print_hex32

    ; Send End Of Interrupt to master PIC
    mov al, 0x20
    out PIC1_CMD, al

    popad
    sti
    iretd

; Common for ALL exceptions after the stub has ensured:
;   [vector][error][eip][cs][eflags]
isr_common:
    cli
    pushad

    lea esi, [esp + 32]        ; ESI = &vector
    cmp dword [esi + 0], 6     ; vector
    jne .no_skip
    add dword [esi + 8], 2     ; saved EIP += 2  (EIP is at +8)

.no_skip:

    ; Load vector+error for show_exception
    mov eax, [esi + 0]         ; vector
    mov ebx, [esi + 4]         ; error

    ; Print EIP/CS/EFLAGS without clobbering vector:
    push eax
    push ebx

    mov eax, [esi + 8]         ; eip
    mov edx, [esi + 12]        ; cs
    mov ecx, [esi + 16]        ; eflags

    pop ebx
    pop eax

    popad
    add esp, 8
    iretd


; -----------------------
; Exception display
; IN:  EAX = vector
;      EBX = error code (unused for now)
; -----------------------
show_exception:
    ; Row 6: "V=0x" + vector
    mov edi, 0xB8000 + (80*6*2)
    mov bl, 0x0E

    push eax
    mov esi, msg_vec
    call vga_print
    pop eax
    call vga_print_hex32

    ; Row 7: friendly name
    mov edi, 0xB8000 + (80*7*2)
    mov bl, 0x0E

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
;      BL  = attribute byte
; OUT: EDI advanced
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
; Print a 32-bit value in EAX as 8 hex chars at [EDI]
; IN:  EAX = value
;      EDI = destination
;      BL  = attribute
; OUT: EDI advanced by 16 bytes
; -----------------------
vga_print_hex32:
    push eax
    push ecx
    push edx

    mov edx, eax          ; working copy (so we don't care about AH etc)
    mov ecx, 8

.hex_loop:
    mov eax, edx
    shr eax, 28
    and eax, 0xF

    add al, '0'
    cmp al, '9'
    jle .ok
    add al, 7             ; 'A' - '9' - 1
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

; -----------------------
; PIC + PIT setup
; -----------------------

; Remap the legacy PIC so that:
;   IRQ0..7  -> vectors 0x20..0x27
;   IRQ8..15 -> vectors 0x28..0x2F
; and unmask only IRQ0 (timer) and IRQ1 (keyboard).
pic_remap:
    push ax
    push dx

    ; Start initialization (ICW1)
    mov al, 0x11
    mov dx, PIC1_CMD
    out dx, al
    mov dx, PIC2_CMD
    out dx, al

    ; ICW2 – vector offset
    mov al, 0x20            ; master offset 0x20
    mov dx, PIC1_DATA
    out dx, al
    mov al, 0x28            ; slave offset 0x28
    mov dx, PIC2_DATA
    out dx, al

    ; ICW3 – wiring
    mov al, 0x04            ; master has slave on IRQ2
    mov dx, PIC1_DATA
    out dx, al
    mov al, 0x02            ; slave identity
    mov dx, PIC2_DATA
    out dx, al

    ; ICW4 – 8086 mode
    mov al, 0x01
    mov dx, PIC1_DATA
    out dx, al
    mov dx, PIC2_DATA
    out dx, al

    ; OCW1 – interrupt masks: enable only IRQ0 and IRQ1 on master
    mov al, 11111100b       ; 0 & 1 unmasked, others masked
    mov dx, PIC1_DATA
    out dx, al

    mov al, 11111111b       ; mask all on slave for now
    mov dx, PIC2_DATA
    out dx, al

    pop dx
    pop ax
    ret

; Initialize PIT channel 0 for 100 Hz periodic interrupts.
; PIT input clock is 1193182 Hz, so divisor ≈ 11932.
pit_init_100hz:
    push ax
    push dx

    ; Command: channel 0, lobyte/hibyte, mode 2 (rate generator), binary
    mov al, 00110100b
    mov dx, PIT_COMMAND
    out dx, al

    mov ax, 11932
    mov dx, PIT_CHANNEL0
    out dx, al              ; low byte
    mov al, ah
    out dx, al              ; high byte

    pop dx
    pop ax
    ret

; -----------------------
; Boot info summary
; -----------------------
; Reads the small boot_info struct in low memory and prints:
;   - whether the magic matches
;   - the number of E820 entries discovered
; Uses both VGA text (rows 4–5) and serial if available.
boot_info_print_summary:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi

    mov eax, [boot_info_ptr]
    test eax, eax
    jz .no_info

    ; Read magic into EBP (not EBX — BL is used for VGA attributes)
    mov ebp, [eax + 0]
    mov ecx, [eax + 16]        ; memmap_len

    ; VGA: row 4 — E820 entry count
    mov edi, 0xB8000 + (80*4*2)
    mov bl, 0x0A
    mov esi, msg_bi_prefix
    call vga_print
    mov eax, ecx
    call vga_print_hex32

    ; VGA: row 5 — magic check (compare EBP, not EBX)
    mov edi, 0xB8000 + (80*5*2)
    mov bl, 0x0A
    cmp ebp, BOOTINFO_MAGIC
    jne .bad_magic
    mov esi, msg_bi_magic_ok
    jmp .print_line
.bad_magic:
    mov esi, msg_bi_magic_bad
.print_line:
    call vga_print

    ; Serial: log entry count
    mov esi, serial_msg_bi
    call serial_write_str
    mov eax, ecx
    call serial_write_hex32
    mov esi, serial_msg_crlf
    call serial_write_str

    jmp .done

.no_info:
    mov edi, 0xB8000 + (80*4*2)
    mov bl, 0x0C
    mov esi, msg_bi_none
    call vga_print

.done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; -----------------------
; E820 memory map display
; -----------------------
; Iterates the E820 array pointed to by boot_info, logs each entry
; over serial, accumulates usable (type 1) RAM, and shows the total
; on VGA row 6.  EBX = current entry pointer throughout the loop.
print_memory_map:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp

    mov eax, [boot_info_ptr]
    test eax, eax
    jz .mm_done

    mov ecx, [eax + 16]        ; memmap_len (entry count)
    test ecx, ecx
    jz .mm_done

    mov ebx, [eax + 12]        ; memmap_ptr -> EBX
    test ebx, ebx
    jz .mm_done

    ; Serial: header line
    push ecx
    mov esi, serial_msg_e820_hdr
    call serial_write_str
    pop ecx

    xor ebp, ebp               ; usable-RAM byte accumulator (32-bit)

.mm_loop:
    push ecx

    mov esi, serial_msg_e820_base
    call serial_write_str
    mov eax, [ebx + 4]         ; base_hi
    call serial_write_hex32
    mov eax, [ebx + 0]         ; base_lo
    call serial_write_hex32

    mov esi, serial_msg_e820_len
    call serial_write_str
    mov eax, [ebx + 12]        ; len_hi
    call serial_write_hex32
    mov eax, [ebx + 8]         ; len_lo
    call serial_write_hex32

    mov esi, serial_msg_e820_type
    call serial_write_str
    mov eax, [ebx + 16]        ; type
    call serial_write_hex32

    mov esi, serial_msg_crlf
    call serial_write_str

    ; type 1 = usable RAM
    cmp dword [ebx + 16], 1
    jne .mm_not_usable
    add ebp, [ebx + 8]         ; accumulate len_lo (safe for < 4 GiB)
.mm_not_usable:

    add ebx, E820_ENTRY_SIZE
    pop ecx
    dec ecx
    jnz .mm_loop

    ; Serial: total usable line
    mov esi, serial_msg_ram_total
    call serial_write_str
    mov eax, ebp
    call serial_write_hex32
    mov esi, serial_msg_crlf
    call serial_write_str

    ; VGA row 6: "Usable RAM: 0xHHHHHHHH"
    mov edi, 0xB8000 + (80*6*2)
    mov bl, 0x0B
    mov esi, msg_ram_total
    call vga_print
    mov eax, ebp
    call vga_print_hex32

.mm_done:
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; -----------------------
; Serial (COM1) logging
; -----------------------

; Initialize COM1 at 0x3F8 for 115200 8N1.
; Uses standard PC UART programming sequence.
serial_init:
    push eax
    push edx

    ; Disable all interrupts
    mov dx, COM1_PORT + 1
    mov al, 0x00
    out dx, al

    ; Enable DLAB to set baud divisor
    mov dx, COM1_PORT + 3
    mov al, 0x80
    out dx, al

    ; Divisor = 1 (115200 baud assuming 115200 base clock)
    mov dx, COM1_PORT + 0
    mov al, 0x01
    out dx, al
    mov dx, COM1_PORT + 1
    mov al, 0x00
    out dx, al

    ; 8 bits, no parity, one stop bit, clear DLAB
    mov dx, COM1_PORT + 3
    mov al, 0x03
    out dx, al

    ; Enable FIFO, clear them, 14-byte threshold
    mov dx, COM1_PORT + 2
    mov al, 0xC7
    out dx, al

    ; IRQs disabled at CPU level for now; set RTS/DSR
    mov dx, COM1_PORT + 4
    mov al, 0x0B
    out dx, al

    pop edx
    pop eax
    ret

; Wait until transmitter holding register is empty.
; Clobbers EAX, EDX.
serial_wait_tx_empty:
    mov dx, COM1_PORT + 5
.wait:
    in  al, dx
    test al, 0x20          ; THR empty?
    jz  .wait
    ret

; Write character in AL to COM1.
; Preserves all registers except EAX/EDX/EBX.
serial_write_char:
    push ebx
    push edx

    mov bl, al             ; save character
    call serial_wait_tx_empty

    mov dx, COM1_PORT
    mov al, bl
    out dx, al

    pop edx
    pop ebx
    ret

; Write zero-terminated string at ESI to COM1.
; Clobbers EAX, EBX, EDX; preserves others.
serial_write_str:
    push esi
.loop:
    lodsb
    test al, al
    jz .done
    call serial_write_char
    jmp .loop
.done:
    pop esi
    ret

; Write 32-bit value in EAX as 8 hex digits over serial.
; Clobbers EAX/EBX/ECX/EDX.
serial_write_hex32:
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
    jle .hex_ok
    add al, 7
.hex_ok:
    call serial_write_char
    shl edx, 4
    loop .hex_loop

    pop edx
    pop ecx
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
; IN:  EDI = VGA destination (cursor)
;      EBX = frame pointer
dump_stack5_ptr_ebx:
    push eax
    push ebx
    push esi
    push ebp

    mov ebp, ebx          ; frame pointer in EBP (do NOT touch EDI)
    mov bl, 0x0E          ; VGA attribute

    mov esi, s0
    call vga_print
    mov eax, [ebp + 0]
    call vga_print_hex32

    mov esi, s1
    call vga_print
    mov eax, [ebp + 4]
    call vga_print_hex32

    mov esi, s2
    call vga_print
    mov eax, [ebp + 8]
    call vga_print_hex32

    mov esi, s3
    call vga_print
    mov eax, [ebp + 12]
    call vga_print_hex32

    mov esi, s4
    call vga_print
    mov eax, [ebp + 16]
    call vga_print_hex32

    pop ebp
    pop esi
    pop ebx
    pop eax
    ret

; Prints:
;   EIP=0x........
;   CS =0x........
;   FL =0x........
; IN: eax=eip, edx=cs, ecx=eflags
print_frame:
    push eax
    push edx
    push ecx

    ; --- EIP ---
    mov edi, 0xB8000 + (80*11*2)
    mov bl, 0x0E
    mov esi, msg_eip
    call vga_print
    pop ecx
    pop edx
    pop eax
    call vga_print_hex32

    ; --- CS ---
    push eax
    push edx
    push ecx
    mov edi, 0xB8000 + (80*12*2)
    mov bl, 0x0E
    mov esi, msg_cs2
    call vga_print
    pop ecx
    pop edx
    pop eax
    mov eax, edx
    call vga_print_hex32

    ; --- FL ---
    mov edi, 0xB8000 + (80*13*2)
    mov bl, 0x0E
    mov esi, msg_fl
    call vga_print
    mov eax, ecx
    call vga_print_hex32

    ret

section .rodata
msg_boot            db "Kernel @ 1MB: boot OK", 0
msg_idt_loaded      db "IDT + PIC + PIT ready, IRQs on", 0
msg_after           db "Returned from #UD successfully!", 0
msg_vec             db "V=0x", 0
msg_unknown         db "Unknown exception", 0
msg_ud              db "#UD Invalid Opcode (vector 6)", 0
msg_gp              db "#GP General Protection Fault (vector 13)", 0
msg_pf              db "#PF Page Fault (vector 14)", 0
msg_eip             db "EIP=0x", 0
msg_cs2             db "CS =0x", 0
msg_fl              db "FL =0x", 0
msg_ticks           db "Ticks: 0x", 0
msg_key             db "Key scancode: 0x", 0
msg_bi_prefix       db "E820 entries: 0x", 0
msg_bi_magic_ok     db "boot_info magic OK", 0
msg_bi_magic_bad    db "boot_info magic BAD ", 0
msg_bi_none         db "boot_info missing", 0

serial_msg_hello    db "serial: kernel start", 13, 10, 0
serial_msg_irq_on   db "kernel: PIC+PIT ready, sti", 13, 10, 0
serial_msg_bi       db "bootinfo: entries=0x", 0
serial_msg_crlf     db 13, 10, 0

serial_msg_e820_hdr  db "== E820 Memory Map ==", 13, 10, 0
serial_msg_e820_base db "  base=", 0
serial_msg_e820_len  db " len=", 0
serial_msg_e820_type db " type=", 0
serial_msg_ram_total db "Usable RAM: 0x", 0
msg_ram_total        db "Usable RAM: 0x", 0

; The `s*` variables represent debug text identifiers.
s0          db " S0=",0
s1          db " S1=",0
s2          db " S2=",0
s3          db " S3=",0
s4          db " S4=",0

section .data
    align 8
    idt:    times IDT_SIZE db 0            ; Define the IDT with 256 entries
    idtr:   dw IDT_SIZE - 1
            dd idt                         ; IDTR points to the start of the IDT