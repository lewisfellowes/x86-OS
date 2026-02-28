bits 32
global task_switch

; void task_switch(uint32_t *old_esp, uint32_t new_esp)
;   [esp+4] = pointer to old process's saved ESP
;   [esp+8] = new process's saved ESP
task_switch:
    ; Save callee-saved registers
    push ebp
    push edi
    push esi
    push ebx

    ; Save current ESP into *old_esp
    mov eax, [esp + 20]     ; old_esp pointer (4 pushes + ret addr = 20)
    mov [eax], esp

    ; Load new ESP
    mov esp, [esp + 24]     ; new_esp (offset shifts due to pushes)

    ; Restore callee-saved registers
    pop ebx
    pop esi
    pop edi
    pop ebp
    ret
