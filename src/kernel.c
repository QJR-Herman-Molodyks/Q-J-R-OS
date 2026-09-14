#define VGA_MEMORY ((volatile unsigned short*)0xB8000)

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define INPUT_SIZE 64

#include "include/io.h"
#include "include/stdint.h"

// calculator
extern void calculator(void);
extern void calc(char* expression);

extern void print_int(int n);

// ATA
extern void init_ata(void);
extern void ata_ls(void);

extern void ata_write(char* filename);
extern void ata_read(char* filename);
extern void ata_delete(char* filename);

extern void ata_stat(char* filename);

extern void mkdir(char* path);
extern void chdir(char* path);
extern void pwd(void);

// IDT

extern void init_idt(void);
extern int keyboard_has_char(void);
extern unsigned char keyboard_pop_scancode(void);

// Writer
extern void writer_open(const char* filename);

// Memory Management
extern unsigned int detect_memory_mb(void);
extern void pmm_init(unsigned int total_ram_mb);
extern void kmalloc_init(void);
extern void* kmalloc(unsigned int size);
extern void kfree(void* ptr);

// config and DB vars
char input[INPUT_SIZE];
static char cut_text_buffer[INPUT_SIZE];

int input_pos = 0;

int cursor_x = 0;
int cursor_y = 0;

int shift_pressed = 0;

// drivers

extern int serial_init(void);
extern void serial_print(const char* str);
extern void beep(void);
extern void speaker_play_sound(uint32_t frequency);
extern void speaker_mute(void);

// ACPI power off

extern void acpi_power_off(void);

// sound

extern void sound_boot(void);

// os info
static char name[] = "Q-J-R OS";
static char version[] = "3.3.1";

// timer

extern void sleep(int seconds);

// architecture
int max_32bit = 2147483647;

unsigned char color = 0x1F;

const char keyboard_map[] = {
    0,
    0,
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    '-', '=',
    '\b',
    '\t',

    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']',
    '\n',

    0,

    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'',
    '`',

    0,
    '\\',

    'z', 'x', 'c', 'v', 'b', 'n', 'm',
    ',', '.', '/',

    0,
    '*',

    0,
    ' '
};

const char keyboard_map_upper[] = {
    0,
    0,
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    '_', '+',
    '\b',
    '\t',

    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}',
    '\n',

    0,

    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    ':', '\"',
    '~',

    0,
    '\\',

    'Z', 'X', 'C', 'V', 'B', 'N', 'M',
    '<', '>', '?',

    0,
    '*',

    0,
    ' '
};

// Scroll down
void scroll_down(void) {
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] =
                VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            ((unsigned short)color << 8) | ' ';
    }
    cursor_y = VGA_HEIGHT - 1;
}

void put_char(char c)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;

        if (cursor_y >= VGA_HEIGHT) {
            scroll_down();
        }

        return;
    }

    VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] =
        ((unsigned short)color << 8) | c;

    cursor_x++;

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= VGA_HEIGHT) {
        scroll_down();
    }
}

void print(const char* str)
{
    while (*str) {
        put_char(*str);
        str++;
    }
}

void clear_screen(void)
{
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] =
                ((unsigned short)color << 8) | ' ';
        }
    }

    cursor_x = 0;
    cursor_y = 0;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *(unsigned char*)a - *(unsigned char*)b;
}

void update_cursor(void) {
    unsigned short position =
        cursor_y * VGA_WIDTH + cursor_x;

    // Low byte
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)0x0F),
          "Nd"((unsigned short)0x3D4)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)(position & 0xFF)),
          "Nd"((unsigned short)0x3D5)
    );

    // High byte
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)0x0E),
          "Nd"((unsigned short)0x3D4)
    );

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)((position >> 8) & 0xFF)),
          "Nd"((unsigned short)0x3D5)
    );
}

// reboot
void reboot(void)
{
    __asm__ volatile ("cli");

    unsigned char status;

    do {
        __asm__ volatile (
            "inb $0x64, %0"
            : "=a"(status)
        );
    } while (status & 0x02);

    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"((unsigned char)0xFE),
          "Nd"((unsigned short)0x64)
    );

    while (1) {
        __asm__ volatile ("hlt");
    }
}

// VGA Draw Rect

void vga_fill_rect(int start_x, int start_y, int end_x, int end_y, int width, int height, char c, unsigned char color_attr) {
    unsigned short entry = ((unsigned short)color_attr << 8) | (unsigned char)c;

    // for (int y = start_y; y < start_y + height; y++) {
    //    for (int x = start_x; x < start_x + width; x++) {
	for (int y = start_y; y < start_y + end_y; y++) {
		for (int x = start_x; x < start_x + end_x; x++) {
            if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
                VGA_MEMORY[y * VGA_WIDTH + x] = entry;
            }
        }
    }
}

// time
unsigned char cmos_read(unsigned char reg)
{
    unsigned char value;

    __asm__ volatile (
        "outb %0, $0x70"
        :
        : "a"(reg)
    );

    __asm__ volatile (
        "inb $0x71, %0"
        : "=a"(value)
    );

    return value;
}

unsigned char bcd_to_bin(unsigned char value)
{
    return (value & 0x0F) + ((value >> 4) * 10);
}

void print_two_digits(unsigned char value)
{
    put_char('0' + value / 10);
    put_char('0' + value % 10);
}

static void print_time(void)
{
    unsigned char hours   = bcd_to_bin(cmos_read(0x04));
    unsigned char minutes = bcd_to_bin(cmos_read(0x02));
    unsigned char seconds = bcd_to_bin(cmos_read(0x00));

    print("Time > ");

    print_two_digits(hours);
    put_char(':');
    print_two_digits(minutes);
    put_char(':');
    print_two_digits(seconds);

    put_char('\n');
}

int len(const char* text) {
    int length = 0;

    for (int i = 0; text[i] != '\0'; i++) {
        length++;
    }

    return length;
}

char* cut_text(const char* text, int index_start, int index_end) {
    int text_length = len(text);

    if (index_start < 0)
        index_start = 0;

    if (index_end >= text_length)
        index_end = text_length - 1;

    if (index_start > index_end)
        return "";

    int j = 0;

    for (int i = index_start; i <= index_end; i++) {
        cut_text_buffer[j++] = text[i];
    }

    cut_text_buffer[j] = '\0';

    return cut_text_buffer;
}

int get_first_space_index(const char* text) {
    int place = 0;
    while (text[place] != ' ' && text[place] != '\0') {
        place++;
    }

    if (place >= len(text)) {
        return -1;
    } else {
        return place;
    }
}

int startswith(const char* text, const char* prefix) {
    int matched = 0;
    for (int i = 0; i <= len(prefix); i++) {
        if (text[i] == prefix[i]) {
            matched++;
        }
    }

    if (matched == len(prefix)) {
        return 1;
    } else {
        return 0;
    }
}

char* get_arg(const char* text) {
    int space = get_first_space_index(text);

    if (space == -1) {
        return "";
    }

    return cut_text(text, space + 1, len(text) - 1);
}

static unsigned char keyboard_read(void) {
    return keyboard_pop_scancode();
}

//unsigned char keyboard_read(void) {
//    unsigned char status;

//    do {
//        __asm__ volatile (
//            "inb $0x64, %0"
//            : "=a"(status)
//        );
//    } while (!(status & 1));

//    unsigned char scancode;

//    __asm__ volatile (
//        "inb $0x60, %0"
//        : "=a"(scancode)
//    );
//
//    return scancode;
//}

// RAM Information

void print_memory_info(void) {
    unsigned int ram_mb = detect_memory_mb();

    print("RAM Detected -> ");
    print_int(ram_mb);
    print(" MB\n");
}

// shutdown function

void power_off(void) {
    // 1. If found port in the Motherboard - sending command in real chipset (ACPI Shutdown)
    acpi_power_off();

    // (Reserved fallbacl ports (emulators, hypervisors and VMs))
    // 2. QEMU Method (standard port for a virtual ACPI-device)
    // Sending Sleep Type 5 (S5 = Soft Off) and enabling bit SLP_EN
    outw(0x604, 0x2000);

    // 3. Method for older versions of QEMU & Bochs
    outw(0xB004, 0x2000);

    // 4. Method for VirtualBox
    outw(0x4004, 0x3400);

    // 5. Trying disabling via extended port QEMU debug-exit
    outb(0x501, 0x31);

    // 6. If the Hardware didn't support any port - stopping the processor FOREVER
    print("\nSystem halted. It is now safe to turn off your computer.\n");
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}


// command execution
static void execute_command(void)
{
    input[input_pos] = '\0';

    print("\n");

    if (strcmp(input, "help") == 0) {
        print("Commands:\n");
        print("  help   - show help\n");
        print("  clear  - clear screen\n");
        print("  reboot - reboot system\n");
        print("  exit   - halt system\n");
        print("\n");
        print("  time   - show current time\n");
        print("  info   - show system info\n");
        print("  calc   - calculator\n");
        print("  echo   - echo text\n");
        print("  beep   - beep\n");
        print("\n");
        print("  ata    - init ata\n");
        print("  ls     - get list of files\n");
        print("  write  - write to file\n");
        print("  read   - read from file\n");
        print("  del    - delete file\n");
        print("  stat   - show file information\n");
        print("  mkdir  - create a directory\n");
        print("  cd     - change a directory\n");
        print("  pwd    - print working directory path\n");
		print("\n");
		print("  ram    - Get information about your RAM\n");
    } else if (strcmp(input, "exit") == 0) {
        print("Shutting down Q-J-R OS...\n");
        update_cursor();

        power_off();
    } else if (strcmp(input, "clear") == 0) {
        clear_screen();
    } else if (strcmp(input, "info") == 0) {
        print("=== Q-J-R OS INFO ===\n");
        print("OS      > ");
        print(name);

        print("\n");

        print("Version > ");
        print(version);

		print("\n");

    	unsigned int ram_mb = detect_memory_mb();
		print("RAM Size> ");
		print_int(ram_mb);
		print(" MB");

        print("\n");

    } else if (strcmp(input, "time") == 0) {
        print_time();
    } else if (startswith(input, "echo") == 1) {
        print(get_arg(input));

    } else if (strcmp(input, "reboot") == 0) {
        print("Rebooting...\n");
        update_cursor();
        reboot();
    } else if (strcmp(input, "calc") == 0) {
        calculator();

    } else if (startswith(input, "calc") == 1) {
        char* arg = get_arg(input);
        if (arg != 0) {
            calc(arg);
        }
    } else if (strcmp(input, "") == 0) {
        print("");

    // ATA
    } else if (strcmp(input, "ata") == 0) {
        init_ata();
    } else if (strcmp(input, "ls") == 0) {
        ata_ls();

    } else if (startswith(input, "write") == 1) {
       char* arg = get_arg(input);
       if (arg != 0) {
          writer_open(arg);
       }
    } else if (startswith(input, "read") == 1) {
       char* arg = get_arg(input);
       if (arg != 0) {
          ata_read(arg);
       }
    } else if (startswith(input, "del") == 1) {
       char* arg = get_arg(input);
       if (arg != 0) {
          ata_delete(arg);
       }
    } else if (startswith(input, "stat") == 1) {
        char* arg = get_arg(input);

        if (arg != 0) {
            ata_stat(arg);
        }
    } else if (startswith(input, "mkdir") == 1) {
        char* arg = get_arg(input);

        if (arg != 0) {
            mkdir(arg);
        }
	} else if (startswith(input, "cd") == 1) {
       char* arg = get_arg(input);
       if (arg != 0 && arg[0] != '\0') {
          chdir(arg);
       } else {
          print("Usage: cd <dirname>\n");
       }
    } else if (strcmp(input, "pwd") == 0) {
        pwd();

    } else if (strcmp(input, "echo") == 0) {
        print("echo: Use echo <text> to print anything to the screen!\n");
	} else if (strcmp(input, "ram") == 0) {
		print_memory_info();
    } else if (strcmp(input, "beep") == 0) {
        beep();
        print("Beep!\n");
    } else {
        print("Unknown command.");
    }

    input_pos = 0;

    print("\nQ-J-R OS> ");
    update_cursor();
}

void input_text(const char* prompt, char* buffer, int buffer_size)
{
    int pos = 0;

    print(prompt);

    while (1) {
        unsigned char scancode = keyboard_read();

        update_cursor();

        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
            continue;
        }

        if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = 0;
            continue;
        }

        if (scancode & 0x80)
            continue;

        if (scancode >= sizeof(keyboard_map))
            continue;

        char c;

        if (shift_pressed) {
            c = keyboard_map_upper[scancode];
        } else {
            c = keyboard_map[scancode];
        }

        if (!c)
            continue;

        if (c == '\b') {
            if (pos > 0) {
                pos--;

                cursor_x--;
                put_char(' ');
                cursor_x--;

                update_cursor();
            }

            continue;
        }

        if (c == '\n') {
            buffer[pos] = '\0';

            print("\n");
            update_cursor();

            return;
        }

        if (pos < buffer_size - 1) {
            buffer[pos++] = c;

            put_char(c);
            update_cursor();
        }
    }
}

static void keyboard_process(void)
{
    unsigned char scancode = keyboard_read();

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }

    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }

    if (scancode & 0x80)
        return;

    if (scancode >= sizeof(keyboard_map))
        return;

    char c;

    if (shift_pressed) {
        c = keyboard_map_upper[scancode];
    } else {
        c = keyboard_map[scancode];
    }

    if (!c)
        return;

    if (c == '\b') {
        if (input_pos > 0) {
            input_pos--;

            cursor_x--;

            put_char(' ');
            cursor_x--;

            update_cursor();
        }

        return;
    }

    if (c == '\n') {
        execute_command();
        return;
    }

    if (input_pos < INPUT_SIZE - 1) {
        input[input_pos++] = c;

        put_char(c);
        update_cursor();
    }
}

// main kernel
void kernel_main(void)
{
    clear_screen();

    print("Q-J-R OS\n");
    print("Protected Mode kernel (IDT + IRQ enabled)\n");
    print("-----------------------------------------\n\n");

    // 1. Serial port - at once for debugging
    serial_print("[Q-J-R OS] Kernel booted, COM1 serial active.\n");

	// 2. Load Interruption Table
	init_idt();

	// 3. Enabling Hardware Interruption of CPU (Set Interrupt Flag)
	__asm__ __volatile__("sti");
    serial_print("[Q-J-R OS] IDT and interrupts configured.\n");

    // 4. Sound signal for OS start
    beep();
    serial_print("[Q-J-R OS] Beep!\n");

    // 5. Memory initialization
    unsigned int ram_mb = detect_memory_mb();
    pmm_init(ram_mb);
    kmalloc_init();

    // 6. Play boot sound
    sound_boot();

	// 7. Load Mini-Conhost

   print("Q-J-R OS> ");
   update_cursor();

    while (1) {
        keyboard_process();
    }
}


// main kernel
//void kernel_main(void)
//{
//    clear_screen();

//    print("Q-J-R OS\n");
//    print("Protected Mode kernel (IDT + IRQ enabled)\n");
//    print("-----------------------------------------\n\n");

	// 1. Load Interruption Table
//	init_idt();

	// 2. Enabling Hardware Interruption of CPU (Set Interrupt Flag)
//	__asm__ __volatile__("sti");

	// 3. Load Mini-Conhost

//	vga_fill_rect(0, 5, 7, 4, VGA_WIDTH, VGA_HEIGHT, 'X', 0x4F);
//	update_cursor();

   // print("Q-J-R OS> ");
   // update_cursor();
//	clear_screen();

    // 1. Детектимо RAM
//    unsigned int ram_mb = detect_memory_mb();

    // 2. Ініціалізуємо менеджери пам'яті
//    pmm_init(ram_mb);
//    kmalloc_init();

//    print("Memory Management initialized!\n");

    // 3. Тепер у ядрі можна динамічно виділяти пам'ять:
//    char* my_buffer = (char*)kmalloc(128);
    //my_buffer[0] = 'H';
    //my_buffer[1] = 'i';
    //my_buffer[2] = '\0';
    //print(my_buffer);
    //print("\n");

    //kfree(my_buffer); // Звільняємо пам'ять

    //while (1) {
    //    keyboard_process();
    //}
//}
