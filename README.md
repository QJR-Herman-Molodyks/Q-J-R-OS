# Official Open-Source Q-J-R OS v2.0.1
## GNU Public License v3.0 (GPL-3.0)!

### Source Files:   

1. ata.c
2. boot.asm
3. calculator.c
4. kernel.c
5. kernel_entry.asm
6. writer.c   

7. linker.ld
8. Makefile

### Features:

#### Regular

1. Help Panel - help
2. Clear Screen - clear
3. Shutdown (halting) - exit
4. System Reboot - reboot
5. Information View - info
6. Calculator - calc
7. BIOS CMOS Time - time
8. Printing Text - echo

#### ATA

9. Initialize ATA - ata
10. List of files in a current directory - ls
11. Read file - read
12. Write file - write
13. Delete file - del
14. File statistics - stat

### Compilation

#### Tools (Crosscompilation):

1. i686-elf-binutils
    1.1. i686-elf-gcc
    1.2. i686-elf-ld
    1.3. i686-elf-objcopy

2. nasm   
3. mkfs.fat   
4. qemu  
5. make  

### Details:

**Architecture**   : x86 (i386)  
**Platform**       : Bare-Metal  
**Run Mode of x86**: Protected Mode  
**Graphics**       : VGA Text Mode 80x25   

**ATA Support**    : FAT16 ATA Chain Support   
**SATA Support**   : No (temporary)
**Filesystem**     : FAT16   

**Floopy**         : No   

**OS**             : Q-J-R OS   
**Version**        : 2.0.1   
**Kernel**         : Q-J-R OS Kernel v2.0.1    
**Kernel Type**    : Monolithic Kernel (Not fully)    
**Release Date**   : 2026-08-18     

**Disk Image Size**: 65536 KB   
**Disk Image**     : .img

**Virtual Machine**: QEMU (i386)

**Supports Compilation**: macOS, GNU/Linux   
**Code**                : Open-Source (Free Software)  
**License**             : GPL-3.0 (GNU General Public License v.0)
