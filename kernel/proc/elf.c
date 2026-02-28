#include "proc/elf.h"
#include "lib/string.h"
#include "drivers/serial.h"

bool elf_load(const void *data, uint32_t size, elf_loaded_t *out) {
    if (size < sizeof(elf32_ehdr_t)) return false;

    const elf32_ehdr_t *ehdr = (const elf32_ehdr_t *)data;
    if (ehdr->e_ident_mag != ELF_MAGIC) {
        serial_puts("ELF: bad magic\r\n");
        return false;
    }

    out->entry_point = ehdr->e_entry;

    const uint8_t *base = (const uint8_t *)data;
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const elf32_phdr_t *ph = (const elf32_phdr_t *)
            (base + ehdr->e_phoff + i * ehdr->e_phentsize);

        if (ph->p_type != PT_LOAD) continue;

        uint8_t *dst = (uint8_t *)ph->p_vaddr;
        const uint8_t *src = base + ph->p_offset;

        if (ph->p_filesz)
            memcpy(dst, src, ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            memset(dst + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
    }

    serial_puts("ELF: loaded, entry=0x");
    serial_hex32(out->entry_point);
    serial_puts("\r\n");
    return true;
}
