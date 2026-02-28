/*
 * mkfs_myfs — Host tool to create a MyFS disk image.
 * Usage: mkfs_myfs <output.img> <size_kb> [file1 file2 ...]
 *
 * Builds with the host compiler: cc -o mkfs_myfs tools/mkfs_myfs.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MYFS_MAGIC       0x4D594653
#define MYFS_BLOCK_SIZE  4096
#define MYFS_SECTORS_PER_BLOCK (MYFS_BLOCK_SIZE / 512)
#define MYFS_MAX_FILES   32
#define MYFS_NAME_LEN    28

typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t bitmap_start;
    uint32_t bitmap_blocks;
    uint32_t root_dir_sector;
    uint32_t data_start;
    uint32_t file_count;
    uint8_t  _pad[480];
} __attribute__((packed)) myfs_super_t;

#define MYFS_FLAG_USED 0x01

typedef struct {
    char     name[MYFS_NAME_LEN];
    uint32_t start_block;
    uint32_t size;
    uint8_t  flags;
    uint8_t  _pad[3];
} __attribute__((packed)) myfs_dirent_t;

static void write_zeros(FILE *f, size_t n) {
    uint8_t zero = 0;
    for (size_t i = 0; i < n; i++)
        fwrite(&zero, 1, 1, f);
}

static const char *basename_of(const char *path) {
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <output.img> <size_kb> [file1 ...]\n", argv[0]);
        return 1;
    }

    const char *out_path = argv[1];
    uint32_t size_kb = (uint32_t)atoi(argv[2]);
    uint32_t total_sectors = size_kb * 2;
    int nfiles = argc - 3;

    if (nfiles > MYFS_MAX_FILES) {
        fprintf(stderr, "Too many files (max %d)\n", MYFS_MAX_FILES);
        return 1;
    }

    FILE *f = fopen(out_path, "wb");
    if (!f) { perror("fopen"); return 1; }

    /* Write zeroed image */
    write_zeros(f, (size_t)total_sectors * 512);

    /* Compute layout */
    uint32_t total_blocks = (total_sectors * 512) / MYFS_BLOCK_SIZE;
    uint32_t bitmap_bits = total_blocks;
    uint32_t bitmap_bytes = (bitmap_bits + 7) / 8;
    uint32_t bitmap_sectors = (bitmap_bytes + 511) / 512;

    uint32_t bitmap_start = 1;  /* sector 0 = super */
    uint32_t root_dir_sector = bitmap_start + bitmap_sectors;
    uint32_t dir_sectors = 4;   /* enough for 48 entries */
    uint32_t data_start = root_dir_sector + dir_sectors;
    /* Align data_start to block boundary */
    data_start = ((data_start * 512 + MYFS_BLOCK_SIZE - 1) / MYFS_BLOCK_SIZE)
                 * MYFS_SECTORS_PER_BLOCK;

    /* Write superblock */
    myfs_super_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = MYFS_MAGIC;
    sb.block_size = MYFS_BLOCK_SIZE;
    sb.total_blocks = total_blocks;
    sb.bitmap_start = bitmap_start;
    sb.bitmap_blocks = (bitmap_sectors * 512 + MYFS_BLOCK_SIZE - 1) / MYFS_BLOCK_SIZE;
    sb.root_dir_sector = root_dir_sector;
    sb.data_start = data_start;
    sb.file_count = (uint32_t)nfiles;

    fseek(f, 0, SEEK_SET);
    fwrite(&sb, sizeof(sb), 1, f);

    /* Write file entries and data */
    uint32_t next_block = 0;
    myfs_dirent_t entries[MYFS_MAX_FILES];
    memset(entries, 0, sizeof(entries));

    for (int i = 0; i < nfiles; i++) {
        FILE *src = fopen(argv[3 + i], "rb");
        if (!src) {
            fprintf(stderr, "Cannot open %s\n", argv[3 + i]);
            fclose(f);
            return 1;
        }

        fseek(src, 0, SEEK_END);
        uint32_t fsize = (uint32_t)ftell(src);
        fseek(src, 0, SEEK_SET);

        const char *name = basename_of(argv[3 + i]);
        strncpy(entries[i].name, name, MYFS_NAME_LEN - 1);
        entries[i].start_block = next_block;
        entries[i].size = fsize;
        entries[i].flags = MYFS_FLAG_USED;

        /* Write file data at data_start + next_block * sectors_per_block */
        uint32_t data_offset = (data_start + next_block * MYFS_SECTORS_PER_BLOCK) * 512;
        fseek(f, (long)data_offset, SEEK_SET);

        uint8_t buf[4096];
        uint32_t remaining = fsize;
        while (remaining > 0) {
            size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
            size_t n = fread(buf, 1, chunk, src);
            fwrite(buf, 1, n, f);
            remaining -= (uint32_t)n;
        }

        uint32_t blocks_used = (fsize + MYFS_BLOCK_SIZE - 1) / MYFS_BLOCK_SIZE;
        if (blocks_used == 0) blocks_used = 1;
        next_block += blocks_used;

        fclose(src);
        printf("  added: %-28s %6u bytes (blocks %u-%u)\n",
               name, fsize, entries[i].start_block,
               entries[i].start_block + blocks_used - 1);
    }

    /* Write directory entries */
    fseek(f, (long)(root_dir_sector * 512), SEEK_SET);
    fwrite(entries, sizeof(myfs_dirent_t), MYFS_MAX_FILES, f);

    /* Write bitmap: mark used blocks */
    uint8_t bitmap[512];
    memset(bitmap, 0, sizeof(bitmap));
    for (uint32_t b = 0; b < next_block && b < bitmap_bits; b++)
        bitmap[b / 8] |= (1 << (b % 8));

    fseek(f, (long)(bitmap_start * 512), SEEK_SET);
    fwrite(bitmap, 1, (bitmap_bytes < 512) ? bitmap_bytes : 512, f);

    fclose(f);
    printf("Created %s: %u KB, %d files, %u data blocks used\n",
           out_path, size_kb, nfiles, next_block);
    return 0;
}
