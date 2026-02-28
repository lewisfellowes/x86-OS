#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t  drive_select;
    bool     present;
    uint32_t sector_count;
    char     model[41];
} ata_drive_t;

void ata_init(void);
ata_drive_t *ata_get_drive(int index);
bool ata_read_sectors(ata_drive_t *d, uint32_t lba, uint8_t count, void *buf);
bool ata_write_sectors(ata_drive_t *d, uint32_t lba, uint8_t count, const void *buf);
