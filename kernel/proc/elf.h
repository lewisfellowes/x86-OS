#pragma once
#include <stdint.h>
#include <stdbool.h>

#define ELF_MAGIC 0x464C457F

/* Relevant ELF32 header fields */
typedef struct {
    uint32_t e_ident_mag;
    uint8_t  e_ident_rest[12];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

#define PT_LOAD 1

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

typedef struct {
    uint32_t entry_point;
} elf_loaded_t;

bool elf_load(const void *data, uint32_t size, elf_loaded_t *out);
