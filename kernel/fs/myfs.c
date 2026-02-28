#include "fs/fs.h"
#include "fs/myfs.h"
#include "drivers/ata.h"
#include "drivers/serial.h"
#include "lib/string.h"

static ata_drive_t *disk;
static myfs_super_t super;
static myfs_dirent_t dir_cache[MYFS_MAX_FILES];
static uint8_t bitmap[512];
static uint8_t sector_buf[512];

typedef struct {
    int      used;
    int      dir_idx;
    uint32_t offset;
    int      writable;
} fd_entry_t;

static fd_entry_t fd_table[FS_MAX_FD];

/* ---- low-level disk helpers ---- */

static bool read_sector(uint32_t lba, void *buf) {
    return ata_read_sectors(disk, lba, 1, buf);
}

static bool write_sector(uint32_t lba, const void *buf) {
    return ata_write_sectors(disk, lba, 1, buf);
}

static bool read_block(uint32_t block, void *buf) {
    uint32_t lba = super.data_start + block * MYFS_SECTORS_PER_BLOCK;
    for (int i = 0; i < MYFS_SECTORS_PER_BLOCK; i++) {
        if (!read_sector(lba + (uint32_t)i, (uint8_t *)buf + i * 512))
            return false;
    }
    return true;
}

static bool write_block(uint32_t block, const void *buf) {
    uint32_t lba = super.data_start + block * MYFS_SECTORS_PER_BLOCK;
    for (int i = 0; i < MYFS_SECTORS_PER_BLOCK; i++) {
        if (!write_sector(lba + (uint32_t)i, (const uint8_t *)buf + i * 512))
            return false;
    }
    return true;
}

/* ---- bitmap helpers ---- */

static bool bitmap_test(uint32_t block) {
    return bitmap[block / 8] & (1 << (block % 8));
}

static void bitmap_set(uint32_t block) {
    bitmap[block / 8] |= (1 << (block % 8));
}

static void __attribute__((unused)) bitmap_clear(uint32_t block) {
    bitmap[block / 8] &= ~(1 << (block % 8));
}

static int bitmap_find_contiguous(uint32_t count) {
    uint32_t run = 0;
    uint32_t start = 0;
    for (uint32_t b = 0; b < super.total_blocks; b++) {
        if (!bitmap_test(b)) {
            if (run == 0) start = b;
            run++;
            if (run >= count) return (int)start;
        } else {
            run = 0;
        }
    }
    return -1;
}

/* ---- flush helpers ---- */

static void flush_bitmap(void) {
    write_sector(super.bitmap_start, bitmap);
}

static void flush_dir(void) {
    uint32_t lba = super.root_dir_sector;
    int written = 0;
    for (int s = 0; s < 4 && written < MYFS_MAX_FILES; s++) {
        memset(sector_buf, 0, 512);
        int per_sector = 512 / (int)sizeof(myfs_dirent_t);
        for (int i = 0; i < per_sector && written < MYFS_MAX_FILES; i++)
            ((myfs_dirent_t *)sector_buf)[i] = dir_cache[written++];
        write_sector(lba + (uint32_t)s, sector_buf);
    }
}

static void flush_super(void) {
    write_sector(0, &super);
}

/* ---- directory helpers ---- */

static int find_file(const char *name) {
    for (int i = 0; i < (int)super.file_count; i++) {
        if ((dir_cache[i].flags & MYFS_FLAG_USED) &&
            strcmp(dir_cache[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int create_file(const char *name) {
    if (super.file_count >= MYFS_MAX_FILES) return -1;

    int slot = -1;
    for (int i = 0; i < MYFS_MAX_FILES; i++) {
        if (!(dir_cache[i].flags & MYFS_FLAG_USED)) { slot = i; break; }
    }
    if (slot < 0) return -1;

    int blk = bitmap_find_contiguous(1);
    if (blk < 0) return -1;
    bitmap_set((uint32_t)blk);

    memset(&dir_cache[slot], 0, sizeof(myfs_dirent_t));
    strncpy(dir_cache[slot].name, name, MYFS_NAME_LEN - 1);
    dir_cache[slot].start_block = (uint32_t)blk;
    dir_cache[slot].size = 0;
    dir_cache[slot].flags = MYFS_FLAG_USED;

    if (slot >= (int)super.file_count)
        super.file_count = (uint32_t)(slot + 1);

    flush_bitmap();
    flush_dir();
    flush_super();

    serial_puts("FS: created '");
    serial_puts(name);
    serial_puts("'\r\n");
    return slot;
}

/* ---- public interface ---- */

void fs_init(void) {
    memset(fd_table, 0, sizeof(fd_table));
    memset(bitmap, 0, sizeof(bitmap));

    disk = ata_get_drive(1);
    if (!disk)
        disk = ata_get_drive(0);
    if (!disk) {
        serial_puts("FS: no ATA drive found\r\n");
        return;
    }

    if (!read_sector(0, &super) || super.magic != MYFS_MAGIC) {
        serial_puts("FS: no MyFS found on disk\r\n");
        memset(&super, 0, sizeof(super));
        return;
    }

    read_sector(super.bitmap_start, bitmap);

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
    if (super.magic != MYFS_MAGIC) return -1;

    int idx = find_file(path);
    if (idx < 0) {
        if (!(flags & FS_FLAG_CREATE)) return -1;
        idx = create_file(path);
        if (idx < 0) return -1;
    }

    for (int i = 0; i < FS_MAX_FD; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used     = 1;
            fd_table[i].dir_idx  = idx;
            fd_table[i].offset   = 0;
            fd_table[i].writable = (flags & FS_FLAG_WRITE) ? 1 : 0;
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
    if (fd < 0 || fd >= FS_MAX_FD || !fd_table[fd].used) return -1;
    if (!fd_table[fd].writable) return -1;

    fd_entry_t *f = &fd_table[fd];
    myfs_dirent_t *de = &dir_cache[f->dir_idx];

    uint32_t end_offset = f->offset + count;
    uint32_t cur_blocks = (de->size + MYFS_BLOCK_SIZE - 1) / MYFS_BLOCK_SIZE;
    if (cur_blocks == 0) cur_blocks = 1;
    uint32_t need_blocks = (end_offset + MYFS_BLOCK_SIZE - 1) / MYFS_BLOCK_SIZE;

    if (need_blocks > cur_blocks) {
        uint32_t extra = need_blocks - cur_blocks;
        uint32_t next = de->start_block + cur_blocks;
        for (uint32_t i = 0; i < extra; i++) {
            if (next + i >= super.total_blocks || bitmap_test(next + i))
                return -1;
        }
        for (uint32_t i = 0; i < extra; i++)
            bitmap_set(next + i);
        flush_bitmap();
    }

    uint8_t block_buf[MYFS_BLOCK_SIZE];
    uint32_t bytes_written = 0;

    while (bytes_written < count) {
        uint32_t block_idx = f->offset / MYFS_BLOCK_SIZE;
        uint32_t block_off = f->offset % MYFS_BLOCK_SIZE;
        uint32_t chunk = MYFS_BLOCK_SIZE - block_off;
        if (chunk > count - bytes_written)
            chunk = count - bytes_written;

        if (block_off != 0 || chunk < MYFS_BLOCK_SIZE)
            read_block(de->start_block + block_idx, block_buf);
        else
            memset(block_buf, 0, MYFS_BLOCK_SIZE);

        memcpy(block_buf + block_off, (const uint8_t *)buf + bytes_written, chunk);

        if (!write_block(de->start_block + block_idx, block_buf))
            break;

        bytes_written += chunk;
        f->offset     += chunk;
    }

    if (f->offset > de->size) {
        de->size = f->offset;
        flush_dir();
    }

    return (int)bytes_written;
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
