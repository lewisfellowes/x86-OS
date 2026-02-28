#include "drivers/ata.h"
#include "arch/io.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define ATA_PRIMARY_IO   0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

#define ATA_REG_DATA     0
#define ATA_REG_ERROR    1
#define ATA_REG_SECCOUNT 2
#define ATA_REG_LBA_LO   3
#define ATA_REG_LBA_MID  4
#define ATA_REG_LBA_HI   5
#define ATA_REG_DRIVE    6
#define ATA_REG_CMD      7
#define ATA_REG_STATUS   7

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ     0x20
#define ATA_CMD_WRITE    0x30

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

static ata_drive_t drives[2];

static void ata_wait_bsy(uint16_t io) {
    while (inb(io + ATA_REG_STATUS) & ATA_SR_BSY)
        ;
}

static void ata_wait_drq(uint16_t io) {
    while (!(inb(io + ATA_REG_STATUS) & ATA_SR_DRQ))
        ;
}

static bool ata_identify(ata_drive_t *d) {
    uint16_t io = d->io_base;

    outb(io + ATA_REG_DRIVE, d->drive_select);
    outb(io + ATA_REG_SECCOUNT, 0);
    outb(io + ATA_REG_LBA_LO, 0);
    outb(io + ATA_REG_LBA_MID, 0);
    outb(io + ATA_REG_LBA_HI, 0);
    outb(io + ATA_REG_CMD, ATA_CMD_IDENTIFY);

    uint8_t status = inb(io + ATA_REG_STATUS);
    if (status == 0) return false;

    ata_wait_bsy(io);

    if (inb(io + ATA_REG_LBA_MID) || inb(io + ATA_REG_LBA_HI))
        return false; /* not ATA */

    for (int i = 0; i < 1000; i++) {
        status = inb(io + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return false;
        if (status & ATA_SR_DRQ) break;
    }
    if (!(inb(io + ATA_REG_STATUS) & ATA_SR_DRQ)) return false;

    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(io + ATA_REG_DATA);

    d->sector_count = (uint32_t)id[60] | ((uint32_t)id[61] << 16);

    for (int i = 0; i < 20; i++) {
        d->model[i * 2]     = (char)(id[27 + i] >> 8);
        d->model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }
    d->model[40] = '\0';
    /* Trim trailing spaces */
    for (int i = 39; i >= 0 && d->model[i] == ' '; i--)
        d->model[i] = '\0';

    d->present = true;
    return true;
}

void ata_init(void) {
    drives[0].io_base      = ATA_PRIMARY_IO;
    drives[0].ctrl_base    = ATA_PRIMARY_CTRL;
    drives[0].drive_select = 0xE0;
    drives[0].present      = false;

    drives[1].io_base      = ATA_PRIMARY_IO;
    drives[1].ctrl_base    = ATA_PRIMARY_CTRL;
    drives[1].drive_select = 0xF0;
    drives[1].present      = false;

    /* Software reset */
    outb(ATA_PRIMARY_CTRL, 0x04);
    io_wait(); io_wait(); io_wait(); io_wait();
    outb(ATA_PRIMARY_CTRL, 0x00);

    for (int i = 0; i < 2; i++) {
        if (ata_identify(&drives[i])) {
            serial_puts("ATA: drive ");
            serial_putc('0' + (char)i);
            serial_puts(" [");
            serial_puts(drives[i].model);
            serial_puts("] sectors=0x");
            serial_hex32(drives[i].sector_count);
            serial_puts("\r\n");
        }
    }
}

ata_drive_t *ata_get_drive(int index) {
    if (index < 0 || index > 1) return 0;
    return drives[index].present ? &drives[index] : 0;
}

bool ata_read_sectors(ata_drive_t *d, uint32_t lba, uint8_t count, void *buf) {
    if (!d || !d->present || count == 0) return false;
    uint16_t io = d->io_base;

    ata_wait_bsy(io);

    outb(io + ATA_REG_DRIVE, d->drive_select | ((lba >> 24) & 0x0F));
    outb(io + ATA_REG_SECCOUNT, count);
    outb(io + ATA_REG_LBA_LO, (uint8_t)(lba));
    outb(io + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(io + ATA_REG_LBA_HI, (uint8_t)(lba >> 16));
    outb(io + ATA_REG_CMD, ATA_CMD_READ);

    uint16_t *p = (uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        ata_wait_bsy(io);
        if (inb(io + ATA_REG_STATUS) & ATA_SR_ERR) return false;
        ata_wait_drq(io);
        for (int i = 0; i < 256; i++)
            *p++ = inw(io + ATA_REG_DATA);
    }
    return true;
}

bool ata_write_sectors(ata_drive_t *d, uint32_t lba, uint8_t count, const void *buf) {
    if (!d || !d->present || count == 0) return false;
    uint16_t io = d->io_base;

    ata_wait_bsy(io);

    outb(io + ATA_REG_DRIVE, d->drive_select | ((lba >> 24) & 0x0F));
    outb(io + ATA_REG_SECCOUNT, count);
    outb(io + ATA_REG_LBA_LO, (uint8_t)(lba));
    outb(io + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(io + ATA_REG_LBA_HI, (uint8_t)(lba >> 16));
    outb(io + ATA_REG_CMD, ATA_CMD_WRITE);

    const uint16_t *p = (const uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        ata_wait_bsy(io);
        ata_wait_drq(io);
        for (int i = 0; i < 256; i++)
            outw(io + ATA_REG_DATA, *p++);
        /* Flush */
        outb(io + ATA_REG_CMD, 0xE7);
        ata_wait_bsy(io);
    }
    return true;
}
