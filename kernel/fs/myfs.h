#pragma once
#include <stdint.h>

/*
 * MyFS on-disk layout:
 *   Sector 0      : Superblock
 *   Sector 1..N   : Free block bitmap
 *   Sector N+1    : Root directory (array of dir_entry)
 *   Remaining     : Data blocks (MYFS_BLOCK_SIZE bytes each)
 *
 * All multi-byte fields are little-endian.
 */

#define MYFS_MAGIC       0x4D594653  /* "MYFS" */
#define MYFS_BLOCK_SIZE  4096
#define MYFS_SECTORS_PER_BLOCK (MYFS_BLOCK_SIZE / 512)
#define MYFS_MAX_FILES   32
#define MYFS_NAME_LEN    28

typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t bitmap_start;     /* sector offset of bitmap */
    uint32_t bitmap_blocks;    /* blocks used by bitmap */
    uint32_t root_dir_sector;  /* sector offset of root directory */
    uint32_t data_start;       /* sector offset of first data block */
    uint32_t file_count;
    uint8_t  _pad[480];        /* fill to 512 bytes */
} __attribute__((packed)) myfs_super_t;

#define MYFS_FLAG_USED  0x01

typedef struct {
    char     name[MYFS_NAME_LEN];
    uint32_t start_block;
    uint32_t size;
    uint8_t  flags;
    uint8_t  _pad[3];
} __attribute__((packed)) myfs_dirent_t;
/* sizeof = 28+4+4+1+3 = 40 bytes; 512/40 = 12 entries per sector */
