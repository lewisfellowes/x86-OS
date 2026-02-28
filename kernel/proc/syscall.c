#include "proc/syscall.h"
#include "arch/idt.h"
#include "drivers/serial.h"
#include "proc/process.h"
#include "fs/fs.h"

static void syscall_handler(isr_frame_t *frame) {
    uint32_t num = frame->eax;
    uint32_t a1  = frame->ebx;
    uint32_t a2  = frame->ecx;
    uint32_t a3  = frame->edx;
    int32_t  ret = -1;

    switch (num) {
    case SYS_EXIT:
        process_exit((int)a1);
        break;

    case SYS_WRITE:
        if (a1 == 1) {
            const char *buf = (const char *)a2;
            for (uint32_t i = 0; i < a3; i++)
                serial_putc(buf[i]);
            ret = (int32_t)a3;
        } else {
            ret = fs_write((fd_t)a1, (const void *)a2, a3);
        }
        break;

    case SYS_READ:
        ret = fs_read((fd_t)a1, (void *)a2, a3);
        break;

    case SYS_OPEN:
        ret = fs_open((const char *)a1, (int)a2);
        break;

    case SYS_CLOSE:
        fs_close((fd_t)a1);
        ret = 0;
        break;

    case SYS_YIELD:
        process_yield();
        ret = 0;
        break;

    case SYS_GETPID:
        ret = (int32_t)process_current()->pid;
        break;

    case SYS_READDIR:
        ret = fs_readdir((const char *)a1, (fs_dirent_t *)a2, (int)a3);
        break;

    case SYS_STAT: {
        fs_stat_t st;
        ret = fs_stat((const char *)a1, &st);
        if (ret == 0 && a2)
            *(fs_stat_t *)a2 = st;
        break;
    }

    case SYS_SEEK:
        ret = fs_seek((fd_t)a1, a2);
        break;

    default:
        serial_puts("SYSCALL: unknown num=0x");
        serial_hex32(num);
        serial_puts("\r\n");
        break;
    }

    frame->eax = (uint32_t)ret;
}

void syscall_init(void) {
    syscall_register(syscall_handler);
    serial_puts("SYSCALL: init (INT 0x30)\r\n");
}
