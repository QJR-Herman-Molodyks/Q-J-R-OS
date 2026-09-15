/*
 * Q-J-R OS IDT Driver (Pure C with GCC Interrupt Attribute)
 */

#define IDT_ENTRIES 256
#define KB_BUFFER_SIZE 256

#include "include/io.h"

// static unsigned char kb_buffer[KB_BUFFER_SIZE];
// static int kb_head = 0;
// static int kb_tail = 0;

extern void timer_handler_main(void);

// Writing structure of IDT
struct idt_entry {
    unsigned short base_low;
    unsigned short selector;
    unsigned char  always0;
    unsigned char  flags;
    unsigned short base_high;
} __attribute__((packed));

// Дескриптор для LIDT
struct idt_ptr {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

// Стековий фрейм, який процесор передає у функцію переривання
struct interrupt_frame {
    unsigned int ip;
    unsigned int cs;
    unsigned int flags;
    unsigned int sp;
    unsigned int ss;
};

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

extern void print(const char* str);
extern void put_char(char c);

extern void keyboard_handler_main(void);

/*
 * Черга клавіатури
 */
// int keyboard_has_char(void) {
//     return kb_head != kb_tail;
// }

// unsigned char keyboard_pop_scancode(void) {
//     while (!keyboard_has_char()) {
//         __asm__ volatile ("hlt"); // CPU спить, доки не спрацює IRQ1!
//     }
//     unsigned char sc = kb_buffer[kb_tail];
//     kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
//     return sc;
// }

/*
 * Ремапінг PIC
 */
static void pic_remap(void) {
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();

    outb(0x21, 0x20); io_wait(); // IRQ0..7 -> 0x20..0x27
    outb(0xA1, 0x28); io_wait(); // IRQ8..15 -> 0x28..0x2F

    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();

    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    outb(0x21, 0xFC); // Дозволяємо лише Timer (0) і Keyboard (1)
    outb(0xA1, 0xFF);
}

static void idt_set_gate(unsigned char num, void* handler, unsigned short sel, unsigned char flags) {
    unsigned int base = (unsigned int)handler;
    idt[num].base_low  = (base & 0xFFFF);
    idt[num].base_high = ((base >> 16) & 0xFFFF);
    idt[num].selector  = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

/*
 * Interruption Handlers (on C)
 */

// Exception: Divide Error (ISR 0)
__attribute__((interrupt))
void isr0_handler(struct interrupt_frame* frame) {
    (void)frame;
    print("\n[EXCEPTION: Divide by Zero! System Halted]\n");
    while (1) { __asm__ volatile ("cli; hlt"); }
}

// Exception: General Protection Fault (ISR 13, має error_code)
__attribute__((interrupt))
void isr13_handler(struct interrupt_frame* frame, unsigned int error_code) {
    (void)frame;
    (void)error_code;
    print("\n[EXCEPTION: General Protection Fault! System Halted]\n");
    while (1) { __asm__ volatile ("cli; hlt"); }
}

// IRQ 0: PIT Timer (0x20)
__attribute__((interrupt))
void irq0_handler(struct interrupt_frame* frame) {
    (void)frame;
    // outb(0x20, 0x20); // EOI
    timer_handler_main();
}

// IRQ 1: Keyboard (0x21)
__attribute__((interrupt))
void irq1_handler(struct interrupt_frame* frame) {
//    (void)frame;

//    unsigned char scancode = inb(0x60);

    // Додаємо сканкод у чергу
//    int next_head = (kb_head + 1) % KB_BUFFER_SIZE;
//    if (next_head != kb_tail) {
//        kb_buffer[kb_head] = scancode;
//        kb_head = next_head;
//    }

//    outb(0x20, 0x20); // EOI
    keyboard_handler_main();
}

// Initialization
void init_idt(void) {
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base  = (unsigned int)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    pic_remap();

    // 0x08 = CODE_SEG, 0x8E = 32-bit Interrupt Gate
    idt_set_gate(0,  isr0_handler,  0x08, 0x8E);
    idt_set_gate(13, isr13_handler, 0x08, 0x8E);
    idt_set_gate(32, irq0_handler,  0x08, 0x8E);
    idt_set_gate(33, irq1_handler,  0x08, 0x8E);

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}