#ifndef IO_H
#define IO_H

/*
 * Basic Low-level Port I/O Helpers and Bus opertaions for x86
 */

// Write 8-bit byte to port

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Read 8-bit byte from a port

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Write 16-bit word to a port
static inline void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Read 16-bit word from port (useful for ATA/IDE Hard Drives)
static inline unsigned short inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Write 32-bit double word to a port (for PCI config/ATA)
static inline void outl(unsigned short port, unsigned int val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

// Read 32-bit double word from port (for PCI config)
static inline unsigned int inl(unsigned short port) {
    unsigned int ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// // I/O bus delay via unused port 0x80 (for PIC configuration) (Затримка шини I/O через порожній порт 0x80 (для налаштування PIC))
static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif // IO_H