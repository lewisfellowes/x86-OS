#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_PROCESSES  16
#define PROC_STACK_SIZE 8192

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE,
} proc_state_t;

typedef struct {
    uint32_t     pid;
    uint32_t     esp;
    uint32_t     kernel_stack_top;
    proc_state_t state;
    const char  *name;
    int          exit_code;
} process_t;

typedef void (*task_entry_t)(void);

void       process_init(void);
process_t *process_create(task_entry_t entry, const char *name);
void       process_exit(int code);
void       process_yield(void);
void       schedule(void);
process_t *process_current(void);
int        process_count(void);
