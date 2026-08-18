/*
 * Q-J-R OS Writer v2.0 (Simple Edition)
 */

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define WRITER_MAX_TEXT 16384

#define KEY_ESC 27
#define KEY_F1  0x3B

/*
 * Беремо напряму рідні функції та змінні з kernel.c
 */
extern volatile unsigned short* VGA_MEMORY;
extern int cursor_x;
extern int cursor_y;
extern unsigned char color;
extern const char keyboard_map[];
extern const char keyboard_map_upper[];

extern void clear_screen(void);
extern void update_cursor(void);
extern void print(const char* str);
extern void put_char(char c);

/*
 * ATA functions
 */

int fat16_write_buffer(const char* filename, const char* buffer, unsigned int text_length);

/*
 * Внутрішнє читання клавіші (використовує рідні змінні ядра)
 */
static unsigned char keyboard_read_raw(void) {
    unsigned char status;
    do {
        __asm__ volatile ("inb $0x64, %0" : "=a"(status));
    } while (!(status & 1));

    unsigned char scancode;
    __asm__ volatile ("inb $0x60, %0" : "=a"(scancode));
    return scancode;
}

static unsigned char writer_get_key(void) {
    static int shift = 0;
    while (1) {
        unsigned char scancode = keyboard_read_raw();

        if (scancode == 0x2A || scancode == 0x36) { shift = 1; continue; }
        if (scancode == 0xAA || scancode == 0xB6) { shift = 0; continue; }
        if (scancode & 0x80) continue; // Release

        if (scancode == 0x01) return KEY_ESC;
        if (scancode == 0x3B) return KEY_F1;

        if (scancode < 60) {
            char c = shift ? keyboard_map_upper[scancode] : keyboard_map[scancode];
            if (c) return (unsigned char)c;
        }
    }
}

/*
 * Стан редактора
 */
static char writer_text[WRITER_MAX_TEXT];
static unsigned int writer_length = 0;
static unsigned int writer_cursor = 0;
static char* status_msg = "";


static void print_number(unsigned int val) {
    if (val == 0) { put_char('0'); return; }
    char buf[10];
    int pos = 0;
    while (val > 0) {
        buf[pos++] = '0' + (val % 10);
        val /= 10;
    }
    while (pos > 0) put_char(buf[--pos]);
}

static void writer_render(const char* filename) {
    clear_screen();

    // 1. Заголовок (Header)
    cursor_x = 0;
    cursor_y = 0;
    print(" [Q-J-R Writer v2.0] | ");
    print(filename);
    print(" | [ESC] Exit  [F1] Save\n");

    int screen_x = 0;
    int screen_y = 1;
    int cur_x = 0;
    int cur_y = 1;

    // 2. Текст
    for (unsigned int i = 0; i < writer_length; i++) {
        if (i == writer_cursor) {
            cur_x = screen_x;
            cur_y = screen_y;
        }

        if (writer_text[i] == '\n') {
            screen_x = 0;
            screen_y++;
        } else {
            if (screen_y < VGA_HEIGHT - 1) {
                ((volatile unsigned short*)0xB8000)[screen_y * VGA_WIDTH + screen_x] =
                    ((unsigned short)color << 8) | (unsigned char)writer_text[i];
            }
            screen_x++;
            if (screen_x >= VGA_WIDTH) {
                screen_x = 0;
                screen_y++;
            }
        }
    }

    if (writer_cursor == writer_length) {
        cur_x = screen_x;
        cur_y = screen_y;
    }

    // 3. Підвал (basement): soon will be a notification board
    cursor_x = 0;
    cursor_y = VGA_HEIGHT - 1;
    print(" Chars: ");
    print_number(writer_length);
    if (status_msg[0] != '\0') {
        print("  |  ");
        print(status_msg);
    }

    // 4. Позиція курсора (cursor position)
    cursor_x = cur_x;
    cursor_y = cur_y;
    update_cursor();
}

static void writer_insert(char c) {
    if (writer_length >= WRITER_MAX_TEXT - 1) return;
    for (unsigned int i = writer_length; i > writer_cursor; i--) {
        writer_text[i] = writer_text[i - 1];
    }
    writer_text[writer_cursor++] = c;
    writer_length++;

    status_msg = "";
}

static void writer_backspace(void) {
    if (writer_cursor == 0) return;
    for (unsigned int i = writer_cursor - 1; i < writer_length - 1; i++) {
        writer_text[i] = writer_text[i + 1];
    }
    writer_cursor--;
    writer_length--;

    status_msg = "";
}


void writer_open(const char* filename) {
    if (!filename || filename[0] == '\0') {
        print("Usage: write <filename>\n");
        return;
    }

    writer_length = 0;
    writer_cursor = 0;

    status_msg = "";

    writer_render(filename);

    while (1) {
        unsigned char key = writer_get_key();

        if (key == KEY_ESC) break;
        if (key == KEY_F1) {
            if (fat16_write_buffer(filename, writer_text, writer_length)) {
                status_msg = "SAVED SUCCESSFULLY!";
            } else {
                status_msg = "SAVE ERROR (Disk not mounted?)";
            }
            writer_render(filename);
            continue;
        } // saving, connecting to FAT16

        if (key == '\b') {
            writer_backspace();
        } else if (key == '\n') {
            writer_insert('\n');
        } else if (key >= 32 && key <= 126) {
            writer_insert((char)key);
        }

        writer_render(filename);
    }

    clear_screen();
}