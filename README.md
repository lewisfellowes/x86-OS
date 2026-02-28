# My OS -- x86 Operating System

A 32-bit x86 operating system that boots from BIOS, featuring a modular
freestanding C kernel, graphical desktop environment with a window manager,
built-in applications, an on-disk filesystem with read/write support, and a
process/syscall model. Runs in QEMU.

---

## Features

### Boot Chain
- **MBR boot sector** with INT 13h LBA disk reads (CHS fallback for floppies)
- **Two-stage loader**: boot sector loads `stage2.bin` which parses the kernel ELF
- **32-bit protected mode** with a flat GDT (4 GB code + data segments)
- **A20 line** enabled via fast A20 (port 0x92)
- **E820 memory map** queried from BIOS and passed to kernel via `boot_info` struct
- **VGA ROM font** (8x16, all 256 glyphs) copied to RAM before entering protected mode

### Kernel (freestanding C)
- Modular architecture: arch, memory, drivers, graphics, GUI, filesystem, process management
- No libc dependency -- all library functions (`memset`, `memcpy`, `strcmp`, etc.) are self-contained
- Minimal assembly: only boot/loader, GDT/IDT stubs, ISR/IRQ entry stubs, and context switching

### Memory Management
- **Physical memory manager** -- bitmap-based page frame allocator using E820 data
- **Identity-mapped paging** -- page directory + page tables allocated from PMM
- **Kernel heap** -- `kmalloc`/`kfree`/`kcalloc` with first-fit allocation and forward coalescing

### Hardware Drivers
- **IDT + ISRs** for CPU exceptions with diagnostic output
- **8259A PIC** remapped to vectors 0x20-0x2F
- **PIT timer** at 100 Hz (IRQ0) with global tick counter and `sleep_ms()`
- **PS/2 keyboard** (IRQ1) with scancode ring buffer, ASCII translation, and modifier key tracking (Shift/Ctrl/Alt)
- **PS/2 mouse** (IRQ12) with 3-byte packet parsing
- **Serial port** (COM1, 115200 8N1) for debug logging
- **ATA PIO** disk driver with IDENTIFY, 28-bit LBA read/write for two IDE drives
- **Bochs Graphics Adapter** -- 640x480x32bpp framebuffer with double buffering

### Filesystem
- **MyFS** -- simple custom block-based on-disk filesystem
- Superblock, bitmap allocator, flat directory with up to 32 files
- **Full read/write support** -- `fs_open`, `fs_read`, `fs_write`, `fs_close`, `fs_create`, `fs_stat`, `fs_readdir`
- Block allocation with contiguous extension, bitmap tracking, directory and superblock flush to disk
- Host-side `mkfs_myfs` tool to format and populate disk images
- Sample files bundled from `rootfs/` directory

### Graphics & Desktop
- **Double-buffered rendering** -- all drawing targets a back buffer, single `fb_flip()` to LFB
- **Bitmap font** rendering (VGA ROM 8x16) with transparent background support
- **Desktop** with gradient wallpaper, taskbar, Start button, and live MM:SS clock
- **Desktop icons** for launching applications (Terminal, Files, Editor, Calc, About)
- **Start menu** popup with app launcher entries, toggled by Start button click
- **Mouse cursor** (8x12 arrow) with background save/restore

### Window Manager
- **Window creation/close/move** with title bar, close button, and drop shadow
- **Z-order management** with focus tracking
- **Title bar dragging** for window repositioning
- **Event routing** -- keyboard events to focused window, mouse events to hit-tested window; clicks on windows are consumed (no click-through to desktop icons)
- **Compositor** that redraws desktop background, windows, and start menu overlay
- **Keyboard shortcuts** -- Alt+F4 to close focused window, Escape to dismiss start menu

### Built-in Applications
- **Terminal** -- command-line with `help`, `clear`, `uname`, `echo` commands; 128-line scrollback buffer with Page Up/Down; command history with Up/Down arrows
- **File Browser** -- reads files from MyFS disk, clickable file list with content viewer
- **Text Editor** -- menu bar with **File → New** and **File → Save**; opens/saves files on MyFS; editable buffer with cursor navigation (arrow keys, Home/End, Page Up/Down), insert/delete, status bar (filename, line/col); untitled documents save as `untitled.txt`; Ctrl+S or File → Save to save
- **Calculator** -- 4-function calculator with button grid
- **About** -- displays system info, RAM usage, and uptime

### Process & Syscall Model
- **Cooperative process model** with process table (up to 16 tasks)
- **Context switching** via assembly `task_switch` routine
- **ELF32 loader** for loading executables
- **System call interface** (INT 0x30) with `SYS_EXIT`, `SYS_READ`, `SYS_WRITE`, `SYS_OPEN`, `SYS_CLOSE`, `SYS_YIELD`, `SYS_GETPID`, `SYS_READDIR`, `SYS_STAT`, `SYS_SEEK`

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
kernel/           Modular C kernel (loaded at 0x100000)
  kmain.c           Entry point, subsystem init, event loop
  arch/             GDT, IDT, PIC, PIT, I/O ports, boot_info
  mem/              PMM, paging, kernel heap
  drivers/          Serial, keyboard, mouse, ATA, framebuffer
  gfx/              Font rendering, mouse cursor
  gui/              Desktop, window manager, compositor, widgets, events
  apps/             Terminal, file browser, text editor, calculator, about
  fs/               VFS interface, MyFS implementation
  proc/             Process management, ELF loader, syscalls
  lib/              Freestanding library (string/memory functions)
```

### Memory Map

| Address         | Contents                          |
|-----------------|-----------------------------------|
| `0x0000-0x3FFF` | BIOS / IVT / BDA                  |
| `0x4000-0x4FFF` | VGA 8x16 font (copied by boot)    |
| `0x5100-0x5380` | E820 memory map entries            |
| `0x6000-0x6017` | `boot_info` struct                 |
| `0x7C00-0x7DFF` | Boot sector (loaded by BIOS)       |
| `0x8000+`       | Stage2 loader                      |
| `0x9000+`       | Kernel ELF (raw, pre-parse)        |
| `0x100000+`     | Kernel code/data/bss               |
| After kernel    | PMM bitmap, page tables, heap      |
| `0xFD000000`    | Framebuffer (LFB via PCI BAR0)     |

---

## Source Tree

```
x86-OS/
  boot.asm                  MBR boot sector (512 bytes)
  stage2.asm                Protected-mode ELF loader
  kernel.asm                Legacy monolithic kernel (preserved for reference)
  linker.ld                 Kernel linker script (entry at 0x100000)
  Makefile                  Build system with macOS/Linux toolchain detection
  .clangd                   Clangd configuration for IDE support
  compile_commands.json     Compilation database for clangd

  kernel/
    kmain.c                 Kernel entry point and event loop
    arch/                   CPU architecture (GDT, IDT, PIC, PIT, I/O, boot protocol)
    mem/                    Memory management (PMM, paging, heap)
    drivers/                Hardware drivers (serial, keyboard, mouse, ATA, framebuffer)
    gfx/                    Graphics primitives (font rendering, cursor)
    gui/                    GUI framework (desktop, window manager, compositor, widgets)
    apps/                   Built-in applications (terminal, file browser, editor, calc, about)
    fs/                     Filesystem (VFS interface, MyFS implementation)
    proc/                   Process management (tasks, ELF loader, syscalls)
    lib/                    Freestanding library (string/memory functions)

  tools/
    mkfs_myfs.c             Host tool to create MyFS disk images

  rootfs/                   Files bundled into the filesystem image
    readme.txt, hello.txt, notes.txt, about.txt, license.txt

  build/                    Generated artifacts (os.img, disk.img, .elf, .o)
```

---

## Requirements

- **NASM** -- assembler
- **i686-elf-gcc** -- cross-compiler for freestanding i686 C
- **QEMU** -- x86 emulator
- **GNU Make** -- build automation

### Install (macOS)

```bash
brew install nasm qemu i686-elf-gcc i686-elf-binutils make
```

### Install (Ubuntu / Debian)

```bash
sudo apt install nasm qemu-system-x86 make gcc-i686-linux-gnu binutils-i686-linux-gnu
```

---

## Build & Run

```bash
make              # Build os.img + disk.img
make run-hdd      # Boot as hard disk in QEMU (serial on stdio)
make run-fdd      # Boot as floppy in QEMU
make clean        # Remove build artifacts
```

The boot image is `build/os.img` and the filesystem image is `build/disk.img`.
QEMU runs with both drives attached.

---

## What You'll See

When you run `make run-hdd`, QEMU shows:

1. A **blue gradient desktop** wallpaper
2. Five **desktop icons** (Terminal, Files, Editor, Calc, About) with shadow labels
3. A **taskbar** at the bottom with a Start button and live **MM:SS clock**
4. Click the **Start button** to open a popup menu with all apps listed
5. A **mouse cursor** that follows your movements
6. Click icons or Start menu entries to **open applications** in draggable windows
7. The **Terminal** accepts commands (`help`, `clear`, `uname`, `echo`); scroll back with Page Up/Down; recall commands with Up/Down arrows
8. The **Text Editor** has a File menu (New, Save); type and navigate, then use File → Save or Ctrl+S (untitled saves as `untitled.txt`)
9. Clicks inside a window go to that window only—no click-through to desktop icons
10. The **File Browser** reads real files from the MyFS disk
11. The **Calculator** handles basic arithmetic with a button grid
12. Press **Alt+F4** to close the focused window; **Escape** to dismiss the Start menu

Serial output (in the terminal) shows boot diagnostics: memory map, PMM stats,
paging info, heap size, ATA drive detection, filesystem mount, and more.

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
- [x] Desktop UI (gradient, icons, taskbar, clock)
- [x] Migrate kernel to modular freestanding C
- [x] ATA PIO disk driver
- [x] On-disk filesystem (MyFS) with read/write
- [x] Window manager (create/move/close/focus)
- [x] Built-in apps (terminal, file browser, calculator, about)
- [x] Process model with context switching
- [x] System call interface (INT 0x30)
- [x] Double-buffered rendering
- [x] Organised source tree (arch/mem/drivers/gfx/gui/apps/fs/proc/lib)
- [x] Text editor with file save support
- [x] Start menu with app launcher
- [x] Keyboard shortcuts (Shift/Ctrl/Alt modifiers, Alt+F4)
- [x] Terminal scrollback and command history
- [ ] Preemptive multitasking
- [ ] User-mode programs (ring 3)
- [ ] Network stack (NE2000 / virtio-net)
