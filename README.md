# x86 Bootloader + 32-bit Kernel (Protected Mode)

This project is a minimal x86 OS development starting point.  
It implements a BIOS boot sector that loads a 32-bit kernel from disk, switches
the CPU into protected mode, and executes the kernel, which writes directly to
VGA text memory.

---

## Features (Current)

- BIOS bootable (MBR-style boot sector)
- Runs in 16-bit real mode initially
- Loads kernel from disk using:
  - INT 13h Extensions (LBA) when available
  - CHS fallback for floppy compatibility
- Switches CPU into 32-bit protected mode
- Flat GDT (4GB code + data segments)
- 32-bit kernel entry point
- VGA text output without BIOS
- Boots correctly as:
  - Hard disk
  - Floppy disk (QEMU)

---

## Requirements

- NASM
- QEMU (x86)
- GNU Make

---

On Ubuntu / Debian-based systems:
```bash
sudo apt install nasm qemu-system-x86 make
```

Build
```bash
make
```

This creates a raw disk image at:

```bash
build/os.img
```


#Run
##Boot as hard disk
```bash
make run-hdd
```

##Boot as floppy
```bash
make run-fdd
```


#Both should display:
```bash
Hello from 32-bit protected mode!
```