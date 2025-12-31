ASM=nasm
BUILD=build

STAGE2_SECTORS=4

all: $(BUILD)/os.img

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.bin: boot.asm | $(BUILD)
	$(ASM) -f bin boot.asm -o $(BUILD)/boot.bin

$(BUILD)/stage2.bin: stage2.asm | $(BUILD)
	$(ASM) -f bin stage2.asm -o $(BUILD)/stage2.bin

$(BUILD)/kernel.bin: kernel.asm | $(BUILD)
	$(ASM) -f bin kernel.asm -o $(BUILD)/kernel.bin

$(BUILD)/os.img: $(BUILD)/boot.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin
	dd if=/dev/zero of=$(BUILD)/os.img bs=512 count=2880 status=none
	dd if=$(BUILD)/boot.bin   of=$(BUILD)/os.img conv=notrunc status=none
	dd if=$(BUILD)/stage2.bin of=$(BUILD)/os.img bs=512 seek=1 conv=notrunc status=none
	dd if=$(BUILD)/kernel.bin of=$(BUILD)/os.img bs=512 seek=$$((1+$(STAGE2_SECTORS))) conv=notrunc status=none

run-hdd: $(BUILD)/os.img
	qemu-system-i386 -drive format=raw,file=$(BUILD)/os.img,if=ide -boot c

run-fdd: $(BUILD)/os.img
	qemu-system-i386 -fda $(BUILD)/os.img -boot a

clean:
	rm -rf $(BUILD)
