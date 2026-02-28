#pragma once
#include <stdint.h>

#define SYS_EXIT     0
#define SYS_WRITE    1
#define SYS_READ     2
#define SYS_OPEN     3
#define SYS_CLOSE    4
#define SYS_YIELD    5
#define SYS_GETPID   6
#define SYS_READDIR  7
#define SYS_STAT     8
#define SYS_SEEK     9

void syscall_init(void);
