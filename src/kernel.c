#define VGA_MEMORY ((volatile unsigned short*)0xB8000)

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define INPUT_SIZE 64

// calculator
extern void calculator(void);
extern void calc(char* expression);

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

// Writer
extern void writer_open(const char* filename);

// config and DB vars
char input[INPUT_SIZE];
static char cut_text_buffer[INPUT_SIZE];

int input_pos = 0;

int cursor_x = 0;
int cursor_y = 0;

int shift_pressed = 0;

// os info
static char name[] = "Q-J-R OS";
static char version[] = "2.0.3";

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

unsigned char keyboard_read(void) {
    unsigned char status;

    do {
        __asm__ volatile (
            "inb $0x64, %0"
            : "=a"(status)
        );
    } while (!(status & 1));

    unsigned char scancode;

    __asm__ volatile (
        "inb $0x60, %0"
        : "=a"(scancode)
    );

    return scancode;
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
        print("\n");
        print("  echo   - echo text\n");
        print("\n");
        print("  ata    - init ata\n");
        print("  ls     - get list of files\n");
        print("  write  - write to file\n");
        print("  read   - read from file\n");
        print("  del    - delete file\n");
        print("  stat   - show file information\n");
        print("\n");
        print("  mkdir  - create a directory\n");
        print("  cd     - change a directory\n");
        print("  pwd    - print working directory path\n");
    } else if (strcmp(input, "exit") == 0) {
        print("System halted.");
        update_cursor();

        while (1) {
            __asm__ volatile ("cli");
            __asm__ volatile ("hlt");
        }
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
    print("Protected Mode kernel\n");
    print("----------------------\n\n");

    print("Q-J-R OS> ");
    update_cursor();

    while (1) {
        keyboard_process();
    }
}