.PHONY: build run clean run_nogr

build:
	nasm -f bin src/boot.asm -o build/boot.bin

	i686-elf-gcc \
		-m32 \
		-ffreestanding \
		-fno-pie \
		-fno-stack-protector \
		-fno-builtin \
		-nostdlib \
		-nodefaultlibs \
		-Wall \
		-Wextra \
		-c src/kernel.c \
		-o build/kernel.o

	i686-elf-gcc \
		-m32 \
		-ffreestanding \
		-fno-pie \
		-fno-stack-protector \
		-fno-builtin \
		-nostdlib \
		-nodefaultlibs \
		-Wall \
		-Wextra \
		-c src/calculator.c \
		-o build/calculator.o

	nasm -f elf32 src/kernel_entry.asm \
		-o build/kernel_entry.o

	i686-elf-ld \
		-m elf_i386 \
		-T linker.ld \
		-o build/kernel.elf \
		build/kernel_entry.o \
		build/kernel.o \
		build/calculator.o

	i686-elf-objcopy \
		-O binary \
		build/kernel.elf \
		build/kernel.bin

	cat build/boot.bin build/kernel.bin > build/os.img

	truncate -s 65536 build/os.img

run:
	qemu-system-i386 -drive format=raw,file=build/os.img

run_nogr:
	qemu-system-i386 -drive format=raw,file=build/os.img -nographic

clean:
	rm -rf build/*