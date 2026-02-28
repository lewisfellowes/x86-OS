ASM     = nasm
BUILD   = build

STAGE2_SECTORS = 1

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  CC      := i686-elf-gcc
  LD      := i686-elf-ld
  OBJCOPY := i686-elf-objcopy
else
  CC      := i686-elf-gcc
  LD      := ld
  OBJCOPY := objcopy
endif

HOSTCC  = cc
CFLAGS  = -ffreestanding -O2 -Wall -Wextra -std=c11 -mno-sse -Ikernel
LDFLAGS = -m elf_i386 -T linker.ld

KERNEL_C_SRC := $(shell find kernel -name '*.c' 2>/dev/null)
KERNEL_A_SRC := $(shell find kernel -name '*.asm' 2>/dev/null)
KERNEL_C_OBJ := $(patsubst kernel/%.c,$(BUILD)/kernel/%.o,$(KERNEL_C_SRC))
KERNEL_A_OBJ := $(patsubst kernel/%.asm,$(BUILD)/kernel/%.o,$(KERNEL_A_SRC))
KERNEL_OBJ   := $(KERNEL_C_OBJ) $(KERNEL_A_OBJ)

ROOTFS_FILES := $(wildcard rootfs/*)

all: $(BUILD)/os.img $(BUILD)/disk.img

# --- Host tools ---
$(BUILD)/mkfs_myfs: tools/mkfs_myfs.c
	@mkdir -p $(BUILD)
	$(HOSTCC) -Wall -Wextra -std=c11 -o $@ $<

# --- Filesystem image ---
$(BUILD)/disk.img: $(BUILD)/mkfs_myfs $(ROOTFS_FILES)
	$(BUILD)/mkfs_myfs $@ 1024 $(ROOTFS_FILES)

# --- Boot components ---
$(BUILD)/boot.bin: boot.asm
	@mkdir -p $(BUILD)
	$(ASM) -f bin $< -o $@

$(BUILD)/stage2.bin: stage2.asm
	@mkdir -p $(BUILD)
	$(ASM) -f bin $< -o $@

# --- Kernel ---
$(BUILD)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel/%.o: kernel/%.asm
	@mkdir -p $(dir $@)
	$(ASM) -f elf32 $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_OBJ) linker.ld
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJ)

# --- Disk image ---
$(BUILD)/os.img: $(BUILD)/boot.bin $(BUILD)/stage2.bin $(BUILD)/kernel.elf
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=$(BUILD)/boot.bin   of=$@ conv=notrunc status=none
	dd if=$(BUILD)/stage2.bin of=$@ bs=512 seek=1 conv=notrunc status=none
	dd if=$(BUILD)/kernel.elf of=$@ bs=512 seek=$$((1+$(STAGE2_SECTORS))) conv=notrunc status=none

# --- Run targets ---
run-hdd: $(BUILD)/os.img $(BUILD)/disk.img
	qemu-system-i386 \
		-drive format=raw,file=$(BUILD)/os.img,if=ide,index=0 \
		-drive format=raw,file=$(BUILD)/disk.img,if=ide,index=1 \
		-boot c -serial stdio -no-reboot

run-fdd: $(BUILD)/os.img $(BUILD)/disk.img
	qemu-system-i386 \
		-fda $(BUILD)/os.img \
		-drive format=raw,file=$(BUILD)/disk.img,if=ide,index=1 \
		-boot a -serial stdio -no-reboot

compile_commands.json: $(KERNEL_C_SRC)
	@echo "[" > $@
	@first=1; for f in $(KERNEL_C_SRC); do \
		if [ $$first -eq 0 ]; then echo "  ,"; fi >> $@; \
		printf '  { "directory": "%s", "command": "$(CC) $(CFLAGS) -c %s", "file": "%s" }\n' \
			"$$(pwd)" "$$f" "$$f" >> $@; \
		first=0; \
	done
	@echo "]" >> $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean run-hdd run-fdd
