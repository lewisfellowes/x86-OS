#pragma once
#include <stdint.h>

#define BOOTINFO_MAGIC   0x1BADB002
#define E820_ENTRY_SIZE  20
#define E820_MAX_ENTRIES 32

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} __attribute__((packed)) e820_entry_t;

#define E820_TYPE_USABLE   1
#define E820_TYPE_RESERVED 2
#define E820_TYPE_ACPI     3
#define E820_TYPE_NVS      4
#define E820_TYPE_BAD      5

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t memmap_ptr;
    uint32_t memmap_len;
    uint8_t  boot_drive;
    uint8_t  _pad[3];
} __attribute__((packed)) boot_info_t;
