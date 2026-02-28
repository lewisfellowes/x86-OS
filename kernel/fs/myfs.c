#include "fs/fs.h"
#include "fs/myfs.h"
#include "drivers/ata.h"
#include "drivers/serial.h"
#include "lib/string.h"

/* We use the second ATA drive (slave) as the filesystem disk.
 * The primary (master) holds the boot image. QEMU: -drive file=disk.img ... */

static ata_drive_t *disk;
static myfs_super_t super;
static myfs_dirent_t dir_cache[MYFS_MAX_FILES];
static uint8_t sector_buf[512];

typedef struct {
    int      used;
    int      dir_idx;
    uint32_t offset;
} fd_entry_t;

static fd_entry_t fd_table[FS_MAX_FD];

static bool read_sector(uint32_t lba, void *buf) {
    return ata_read_sectors(disk, lba, 1, buf);
}

static bool read_block(uint32_t block, void *buf) {
    uint32_t lba = super.data_start + block * MYFS_SECTORS_PER_BLOCK;
    for (int i = 0; i < MYFS_SECTORS_PER_BLOCK; i++) {
        if (!ata_read_sectors(disk, lba + (uint32_t)i, 1,
                              (uint8_t *)buf + i * 512))
            return false;
    }
    return true;
}

static int find_file(const char *name) {
    for (int i = 0; i < (int)super.file_count; i++) {
        if ((dir_cache[i].flags & MYFS_FLAG_USED) &&
            strcmp(dir_cache[i].name, name) == 0)
            return i;
    }
    return -1;
}

void fs_init(void) {
    memset(fd_table, 0, sizeof(fd_table));

    disk = ata_get_drive(1);
    if (!disk) {
        disk = ata_get_drive(0);
    }
    if (!disk) {
        serial_puts("FS: no ATA drive found\r\n");
        return;
    }

    if (!read_sector(0, &super) || super.magic != MYFS_MAGIC) {
        serial_puts("FS: no MyFS found on disk\r\n");
        memset(&super, 0, sizeof(super));
        return;
    }

    /* Load root directory (up to 4 sectors = 48 entries, we cap at 32) */
    memset(dir_cache, 0, sizeof(dir_cache));
    uint32_t dir_lba = super.root_dir_sector;
    int loaded = 0;
    for (int s = 0; s < 4 && loaded < MYFS_MAX_FILES; s++) {
        if (!read_sector(dir_lba + (uint32_t)s, sector_buf)) break;
        int per_sector = 512 / (int)sizeof(myfs_dirent_t);
        for (int i = 0; i < per_sector && loaded < MYFS_MAX_FILES; i++)
            dir_cache[loaded++] = ((myfs_dirent_t *)sector_buf)[i];
    }

    serial_puts("FS: MyFS mounted, files=");
    serial_hex32(super.file_count);
    serial_puts("\r\n");
}

fd_t fs_open(const char *path, int flags) {
    (void)flags;
    if (super.magic != MYFS_MAGIC) return -1;

    int idx = find_file(path);
    if (idx < 0) return -1;

    for (int i = 0; i < FS_MAX_FD; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used    = 1;
            fd_table[i].dir_idx = idx;
            fd_table[i].offset  = 0;
            return i;
        }
    }
    return -1;
}

int fs_read(fd_t fd, void *buf, uint32_t count) {
    if (fd < 0 || fd >= FS_MAX_FD || !fd_table[fd].used) return -1;

    fd_entry_t *f = &fd_table[fd];
    myfs_dirent_t *de = &dir_cache[f->dir_idx];

    if (f->offset >= de->size) return 0;
    if (f->offset + count > de->size)
        count = de->size - f->offset;

    uint8_t block_buf[MYFS_BLOCK_SIZE];
    uint32_t bytes_read = 0;

    while (bytes_read < count) {
        uint32_t block_idx = f->offset / MYFS_BLOCK_SIZE;
        uint32_t block_off = f->offset % MYFS_BLOCK_SIZE;
        uint32_t chunk = MYFS_BLOCK_SIZE - block_off;
        if (chunk > count - bytes_read)
            chunk = count - bytes_read;

        if (!read_block(de->start_block + block_idx, block_buf))
            return (int)bytes_read;

        memcpy((uint8_t *)buf + bytes_read, block_buf + block_off, chunk);
        bytes_read += chunk;
        f->offset  += chunk;
    }
    return (int)bytes_read;
}

int fs_write(fd_t fd, const void *buf, uint32_t count) {
    /* Simplified: write not yet fully implemented */
    (void)fd; (void)buf; (void)count;
    return -1;
}

int fs_seek(fd_t fd, uint32_t offset) {
    if (fd < 0 || fd >= FS_MAX_FD || !fd_table[fd].used) return -1;
    fd_table[fd].offset = offset;
    return 0;
}

void fs_close(fd_t fd) {
    if (fd >= 0 && fd < FS_MAX_FD)
        fd_table[fd].used = 0;
}

int fs_stat(const char *path, fs_stat_t *st) {
    if (super.magic != MYFS_MAGIC) return -1;
    int idx = find_file(path);
    if (idx < 0) return -1;
    st->size = dir_cache[idx].size;
    return 0;
}

int fs_readdir(const char *path, fs_dirent_t *entries, int max) {
    (void)path;
    if (super.magic != MYFS_MAGIC) return 0;

    int n = 0;
    for (int i = 0; i < (int)super.file_count && n < max; i++) {
        if (dir_cache[i].flags & MYFS_FLAG_USED) {
            strncpy(entries[n].name, dir_cache[i].name, FS_MAX_NAME);
            entries[n].size  = dir_cache[i].size;
            entries[n].flags = dir_cache[i].flags;
            n++;
        }
    }
    return n;
}

bool fs_exists(const char *path) {
    if (super.magic != MYFS_MAGIC) return false;
    return find_file(path) >= 0;
}
