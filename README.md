# My OS -- x86 Operating System in Assembly

A 32-bit x86 operating system written entirely in NASM assembly, featuring a
graphical desktop environment with a file manager, mouse cursor, and live clock.
Boots from BIOS via a custom MBR boot sector and runs in QEMU.

![Desktop Screenshot](https://via.placeholder.com/640x480?text=My+OS+Desktop)

---

## Features

### Boot & Core
- **MBR boot sector** with INT 13h LBA disk reads (CHS fallback for floppies)
- **Two-stage loader**: boot sector loads `stage2.bin` which parses the kernel ELF
- **32-bit protected mode** with a flat GDT (4 GB code + data segments)
- **A20 line** enabled via fast A20 (port 0x92)
- **E820 memory map** queried from BIOS and passed to kernel via `boot_info` struct
- **VGA ROM font** (8x16, all 256 glyphs) copied to RAM before entering protected mode

### Memory Management
- **Physical memory manager** -- bitmap-based page frame allocator using E820 data
- **Identity-mapped paging** -- page directory + page tables allocated from PMM
- **Kernel heap** -- `kmalloc`/`kfree` with first-fit allocation and forward coalescing

### Hardware Drivers
- **IDT + ISRs** for exceptions (#UD, #GP, #PF) with stack frame display
- **8259A PIC** remapped to vectors 0x20-0x2F
- **PIT timer** at 100 Hz (IRQ0) with global tick counter
- **PS/2 keyboard** (IRQ1) with scancode capture
- **PS/2 mouse** (IRQ12) with 3-byte packet parsing, sign extension, overflow
  rejection, and aux-port data filtering
- **Serial port** (COM1, 115200 8N1) for debug logging

### Graphics & UI
- **Bochs VGA adapter** (BGA) programmed for 640x480x32bpp via I/O ports
- **Linear framebuffer** address read from PCI BAR0, mapped into page tables
- **Primitives**: `fb_fill_rect`, `fb_draw_char`, `fb_draw_string`
- **Transparent text** rendering (for icon labels on gradient backgrounds)
- **Desktop gradient wallpaper** (22-band vertical blue gradient)
- **Desktop icons**: Computer, Files, Trash with drop-shadow labels
- **Taskbar** with green Start button, separator, and live MM:SS uptime clock
- **File Manager window** with drop shadow, 1px border, blue title bar, close button
- **Clickable file list** (5 in-memory demo files) with selection highlighting
- **Content viewer** pane with multi-line text rendering and word wrap
- **Mouse cursor** (8x12 arrow bitmap) with background save/restore

---

## Architecture

```
boot.asm          MBR boot sector (512 bytes)
  |                 - Enables A20, queries E820, copies VGA font
  |                 - Loads stage2 + kernel ELF from disk
  |                 - Enters 32-bit protected mode
  v
stage2.asm        ELF loader (runs at 0x8000)
  |                 - Parses ELF32 program headers
  |                 - Copies kernel segments to 1 MB
  |                 - Passes boot_info pointer in EAX
  v
kernel.asm        Kernel (loaded at 0x100000)
                    - Serial init, memory map, PMM, paging, heap
                    - IDT, PIC, PIT, keyboard, mouse
                    - BGA framebuffer, desktop UI, file manager
                    - Event loop: clock updates + mouse click handling
```

### Memory Map

| Address       | Contents                          |
|---------------|-----------------------------------|
| `0x0000-0x3FFF` | BIOS / IVT / BDA                |
| `0x4000-0x4FFF` | VGA 8x16 font (copied by boot) |
| `0x5100-0x5380` | E820 memory map entries         |
| `0x6000-0x6017` | `boot_info` struct              |
| `0x7C00-0x7DFF` | Boot sector (loaded by BIOS)    |
| `0x8000+`       | Stage2 loader                   |
| `0x9000+`       | Kernel ELF (raw, pre-parse)     |
| `0x100000+`     | Kernel code/data/bss            |
| After kernel    | PMM bitmap, page tables, heap   |
| `0xFD000000`    | Framebuffer (LFB via PCI BAR0)  |

---

## Requirements

- **NASM** -- assembler
- **QEMU** -- x86 emulator
- **GNU Make** -- build automation
- **macOS only:** `i686-elf-binutils` (for cross `ld` and `objcopy`)

### Install (macOS)

```bash
brew install nasm qemu i686-elf-binutils make
```

### Install (Ubuntu / Debian)

```bash
sudo apt install nasm qemu-system-x86 make binutils
```

---

## Build & Run

```bash
make              # Build disk image
make run-hdd      # Boot as hard disk in QEMU (with serial on stdio)
make run-fdd      # Boot as floppy in QEMU
make clean        # Remove build artifacts
```

The disk image is written to `build/os.img` (1.44 MB floppy layout).

---

## File Structure

```
x86-OS/
  boot.asm        Boot sector (MBR, 512 bytes)
  stage2.asm      Protected-mode ELF loader
  kernel.asm      Kernel (all subsystems in one file)
  linker.ld       Kernel linker script (entry at 0x100000)
  Makefile        Build system with macOS/Linux toolchain detection
  build/          Generated artifacts (os.img, .bin, .elf, .o)
```

---

## What You'll See

When you run `make run-hdd`, QEMU shows:

1. A **blue gradient desktop** wallpaper
2. Three **desktop icons** on the left (Computer, Files, Trash) with shadow labels
3. A **File Manager** window with a dark drop shadow and border
4. Five **clickable files** (readme.txt, hello.txt, notes.txt, about.txt, license.txt)
5. A **content viewer** showing the selected file's text
6. A **taskbar** at the bottom with a green Start button and a live **clock** (MM:SS)
7. A **mouse cursor** that follows your mouse movements

Serial output (in the terminal) shows boot diagnostics: memory map, PMM stats,
paging info, heap allocation tests, and framebuffer setup.

---

## Roadmap

- [x] MBR boot sector + stage2 ELF loader
- [x] IDT, PIC, PIT timer, keyboard driver
- [x] E820 memory map + physical memory manager
- [x] Identity-mapped paging
- [x] Kernel heap (kmalloc/kfree)
- [x] 640x480x32bpp framebuffer (Bochs VGA)
- [x] Bitmap font rendering (VGA ROM 8x16)
- [x] PS/2 mouse driver with cursor
- [x] File manager with in-memory files
- [x] Desktop UI (gradient, icons, shadows, clock)
- [ ] ATA PIO disk driver
- [ ] Simple on-disk filesystem
- [ ] Window manager (draggable/resizable windows)
- [ ] User-mode programs
- [ ] Network stack (NE2000 / virtio-net)
