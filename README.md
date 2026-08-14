# Official Open-Source Q-J-R OS v1.5.1
## GNU Public License v3.0 (GPL-3.0)!

### Files:

1. boot.asm
2. calculator.c
3. kernel.c
4. kernel_entry.asm

5. linker.ld
6. Makefile

### Features:

1. Help Panel - help
2. Clear Screen - clear
3. Shutdown (halting) - exit
4. System Reboot - reboot
5. Information View - info
6. Calculator - calc
7. BIOS Time - time
8. Printing Text - echo

### Compilation

#### Tools (Crosscompilation):

1. i686-elf-binutils
    1.1. i686-elf-gcc
    1.2. i686-elf-ld
    1.3. i686-elf-objcopy

2. nasm
3. qemu
4. make

### Details:

**Architecture**   : x86 (i386)  
**Platform**       : Bare-Metal  
**Run Mode of x86**: Protected Mode  
**Graphics**       : VGA Text Mode 80x25   

**ATA Support**    : No (temporary)   
**SATA Support**   : No (temporary)  
**Filesystem**     : None (temporary)

**Floopy**         : No   

**OS**             : Q-J-R OS   
**Version**        : 1.5.1    
**Kernel**         : Q-J-R OS Kernel v1.5.1    
**Kernel Type**    : Monolithic Kernel (Not fully)   
**Release Date**   : 2026-08-13  

**Disk Image Size**: 65536 KB
**Disk Image**     : .img

**Virtual Machine**: QEMU (i386)

**Supports Compilation**: macOS, GNU/Linux
**Code**                : Open-Source
