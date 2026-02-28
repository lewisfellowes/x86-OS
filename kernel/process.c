#include "process.h"
#include "heap.h"
#include "serial.h"
#include "string.h"
#include "io.h"

extern void task_switch(uint32_t *old_esp, uint32_t new_esp);

static process_t procs[MAX_PROCESSES];
static int current_pid;
static int next_pid = 1;

void process_init(void) {
    memset(procs, 0, sizeof(procs));

    /* PID 0 = kernel / idle (uses existing stack) */
    procs[0].pid   = 0;
    procs[0].state = PROC_RUNNING;
    procs[0].name  = "kernel";
    current_pid = 0;

    serial_puts("PROC: init OK\r\n");
}

static void process_entry_wrapper(void) {
    /* The actual entry is stored after the initial frame by process_create.
     * We reach here via task_switch -> ret, and EBX holds the real entry. */
    task_entry_t fn;
    __asm__ volatile ("mov %%ebx, %0" : "=r"(fn));
    fn();
    process_exit(0);
}

process_t *process_create(task_entry_t entry, const char *name) {
    int slot = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (procs[i].state == PROC_UNUSED) { slot = i; break; }
    }
    if (slot < 0) return 0;

    uint8_t *stack = (uint8_t *)kmalloc(PROC_STACK_SIZE);
    if (!stack) return 0;

    process_t *p = &procs[slot];
    p->pid = (uint32_t)next_pid++;
    p->kernel_stack_top = (uint32_t)(stack + PROC_STACK_SIZE);
    p->name = name;
    p->state = PROC_READY;
    p->exit_code = 0;

    /*
     * Set up initial stack frame that task_switch will pop:
     *   [entry_wrapper]  <- return address for 'ret' in task_switch
     *   [ebx = entry]    <- real function pointer
     *   [esi] [edi] [ebp]
     */
    uint32_t *sp = (uint32_t *)(stack + PROC_STACK_SIZE);
    *(--sp) = (uint32_t)process_entry_wrapper; /* return addr */
    *(--sp) = (uint32_t)entry;                 /* ebx */
    *(--sp) = 0;                               /* esi */
    *(--sp) = 0;                               /* edi */
    *(--sp) = 0;                               /* ebp */
    p->esp = (uint32_t)sp;

    serial_puts("PROC: created '");
    serial_puts(name);
    serial_puts("' pid=");
    serial_hex32(p->pid);
    serial_puts("\r\n");
    return p;
}

void process_exit(int code) {
    procs[current_pid].exit_code = code;
    procs[current_pid].state = PROC_ZOMBIE;
    serial_puts("PROC: exit pid=");
    serial_hex32(procs[current_pid].pid);
    serial_puts("\r\n");
    schedule();
    for (;;) hlt();
}

static int find_next_ready(void) {
    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int idx = (current_pid + i) % MAX_PROCESSES;
        if (procs[idx].state == PROC_READY)
            return idx;
    }
    return 0; /* fall back to idle/kernel */
}

void schedule(void) {
    int prev = current_pid;
    int next = find_next_ready();

    if (next == prev) return;

    if (procs[prev].state == PROC_RUNNING)
        procs[prev].state = PROC_READY;

    current_pid = next;
    procs[next].state = PROC_RUNNING;

    task_switch(&procs[prev].esp, procs[next].esp);
}

void process_yield(void) {
    schedule();
}

process_t *process_current(void) {
    return &procs[current_pid];
}

int process_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (procs[i].state != PROC_UNUSED && procs[i].state != PROC_ZOMBIE)
            n++;
    return n;
}
