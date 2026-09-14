# Official Open-Source Q-J-R OS v3.3.1 (Free Software)
## GNU Public License v3.0 (GPL-3.0)!

### Source Files:   

1. acpi.c
2. ata.c  
3. audio.c   
4. boot.asm
5. calculator.c
6. kernel.c
7. kernel_entry.asm
8. keyboard.c   
9. writer.c   
10. idt.c    
11. memory.c   
12. serial.c
13. sound.c   
14. writer.c

15. linker.ld
16. Makefile

### Includes

1. io.h
2. timer.h   
3. sound.h   
4. stdbool.h  
5. stddef.h   
6. stdint.h   

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

##### ATA: Work with Files

10. List of files in a current directory - ls
11. Read file - read
12. Write file - write
13. Delete file - del
14. File statistics - stat

##### ATA: Work with directories

15. Make directory - mkdir
16. Print Working Directory Path - pwd
17. Change the directory - cd

#### Hardware

18. See RAM information - ram

#### Sounds

19. BEEP! - beep

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
**Kernel Type**    : Monolithic Kernel (Not fully)       

**Disk Image Size**: 65536 KB   
**Disk Image**     : .img   

**Virtual Machine**: QEMU (i386)

**Supports Compilation**: macOS, GNU/Linux   
**Code**                : Open-Source (Free Software)  
**License**             : GPL-3.0 (GNU General Public License v.0)
