.PHONY: build run clean run_nogr diagnose run_nod run_nogr

TOTAL_STEPS = "04"

build:
	@echo "[01/$(TOTAL_STEPS)] Compiling a source"

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
		-c src/ata.c \
		-o build/ata.o

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
		-c src/writer.c \
		-o build/writer.o

	i686-elf-gcc \
		-m32 \
	  	-mgeneral-regs-only \
	  	-ffreestanding \
	  	-fno-pie \
	  	-fno-stack-protector \
		-fno-builtin \
		-nostdlib \
		-nodefaultlibs \
		-Wall \
		-Wextra \
		-c src/idt.c \
		-o build/idt.o

	i686-elf-gcc \
		-m32 \
		-mgeneral-regs-only \
		-ffreestanding \
		-fno-pie \
		-fno-stack-protector \
		-fno-builtin \
		-nostdlib \
		-nodefaultlibs \
		-Wall \
		-Wextra \
		-c src/memory.c \
		-o build/memory.o


	i686-elf-gcc \
		   -m32 \
		   -mgeneral-regs-only \
		   -ffreestanding \
		   -fno-pie \
		   -fno-stack-protector \
		   -fno-builtin \
		   -nostdlib \
		   -nodefaultlibs \
		   -Wall \
		   -Wextra \
		   -c src/serial.c \
		   -o build/serial.o


	i686-elf-gcc \
		   -m32 \
		   -mgeneral-regs-only \
		   -ffreestanding \
		   -fno-pie \
		   -fno-stack-protector \
		   -fno-builtin \
		   -nostdlib \
		   -nodefaultlibs \
		   -Wall \
		   -Wextra \
		   -c src/keyboard.c \
		   -o build/keyboard.o


	i686-elf-gcc \
		   -m32 \
		   -mgeneral-regs-only \
		   -ffreestanding \
		   -fno-pie \
		   -fno-stack-protector \
		   -fno-builtin \
		   -nostdlib \
		   -nodefaultlibs \
		   -Wall \
		   -Wextra \
		   -c src/audio.c \
		   -o build/audio.o

	i686-elf-gcc \
		   -m32 \
		   -mgeneral-regs-only \
		   -ffreestanding \
		   -fno-pie \
		   -fno-stack-protector \
		   -fno-builtin \
		   -nostdlib \
		   -nodefaultlibs \
		   -Wall \
		   -Wextra \
		   -c src/acpi.c \
		   -o build/acpi.o

	i686-elf-gcc \
		   -m32 \
		   -mgeneral-regs-only \
		   -ffreestanding \
		   -fno-pie \
		   -fno-stack-protector \
		   -fno-builtin \
		   -nostdlib \
		   -nodefaultlibs \
		   -Wall \
		   -Wextra \
		   -c src/sound.c \
		   -o build/sound.o

	i686-elf-gcc \
		   -m32 \
		   -mgeneral-regs-only \
		   -ffreestanding \
		   -fno-pie \
		   -fno-stack-protector \
		   -fno-builtin \
		   -nostdlib \
		   -nodefaultlibs \
		   -Wall \
		   -Wextra \
		   -c src/timer.c \
		   -o build/timer.o


	nasm -f elf32 src/kernel_entry.asm \
		-o build/kernel_entry.o

	echo "[02/$(TOTAL_STEPS)] Linking a source"

	i686-elf-ld \
		-m elf_i386 \
		-T linker.ld \
		-o build/kernel.elf \
		build/kernel_entry.o \
		build/kernel.o \
		build/calculator.o \
		build/ata.o \
		build/writer.o \
		build/idt.o \
		build/memory.o \
		build/serial.o \
		build/keyboard.o \
		build/audio.o \
		build/acpi.o \
		build/sound.o \
		build/timer.o

	@echo "[02/$(TOTAL_STEPS)] Objcopying..."


	i686-elf-objcopy \
		-O binary \
		build/kernel.elf \
		build/kernel.bin

	@echo "[03/$(TOTAL_STEPS)] Creating FAT16 drive..."

	truncate -s 16M dsk/fat16.img
	mkfs.fat -F 16 dsk/fat16.img

	@echo "[04/$(TOTAL_STEPS)] Creating OS Image."

	 cat build/boot.bin build/kernel.bin > build/os.img
#	os.img: build/boot.bin build/kernel.bin
#		cat build/boot.bin build/kernel.bin > os.img
#		truncate -s 64K os.img

	#truncate -s 131072 build/os.img
	truncate -s 128K build/os.img

diagnose:
	ls -lh build/kernel.bin
	wc -c build/kernel.bin

run:
	qemu-system-i386 \
		-drive file=build/os.img,format=raw,index=0,media=disk \
		-drive file=dsk/fat16.img,format=raw,index=1,media=disk \
		-serial stdio \
		-boot order=c

run_nod:
	qemu-system-i386 -drive format=raw,file=build/os.img

run_nogr:
	qemu-system-i386 -drive format=raw,file=build/os.img -nographic

clean:
	rm -rf build/*
	rm -rf dsk/*
