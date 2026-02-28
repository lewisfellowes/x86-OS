#pragma once
#include <stdint.h>
#include <stdbool.h>

#define FS_MAX_NAME   28
#define FS_MAX_FD     16
#define FS_MAX_DIRENT 32

#define FS_FLAG_READ  0x01
#define FS_FLAG_WRITE 0x02

typedef int fd_t;

typedef struct {
    char     name[FS_MAX_NAME];
    uint32_t size;
    uint8_t  flags;
} fs_dirent_t;

typedef struct {
    uint32_t size;
} fs_stat_t;

void  fs_init(void);
fd_t  fs_open(const char *path, int flags);
int   fs_read(fd_t fd, void *buf, uint32_t count);
int   fs_write(fd_t fd, const void *buf, uint32_t count);
int   fs_seek(fd_t fd, uint32_t offset);
void  fs_close(fd_t fd);
int   fs_stat(const char *path, fs_stat_t *st);
int   fs_readdir(const char *path, fs_dirent_t *entries, int max);
bool  fs_exists(const char *path);
