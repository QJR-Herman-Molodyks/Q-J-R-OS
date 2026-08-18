/*
 * Q-J-R OS Writer v2.0
 */

extern void print(const char* str);
extern void put_char(char c);
extern void update_cursor(void);

/*
 * Writer limits
 */

#define WRITER_MAX_TEXT 16384
#define WRITER_SCREEN_TOP 1
#define WRITER_SCREEN_BOTTOM 24

/*
 * Special keys
 */

#define WRITER_KEY_ESC  27
#define WRITER_KEY_F1   0x3B

/*
 * Editor state
 */

static char writer_text[WRITER_MAX_TEXT];

static unsigned int writer_length = 0;
static unsigned int writer_cursor = 0;

static unsigned int writer_line = 0;
static unsigned int writer_column = 0;

static unsigned int writer_scroll = 0;


/*
 * Clear editor screen.
 *
 * Поки що тут залишаємо простий варіант.
 * Пізніше підключимо VGA clear_screen().
 */

static void writer_clear_screen(void)
{
    /*
     * TODO:
     * Підключити функцію очищення VGA.
     */
}


/*
 * Draw top bar.
 */

static void writer_draw_header(const char* filename)
{
    print("Q-J-R OS Writer v2.0 | ");
    print(filename);
    print(" | ESC = Escape   F1 = Save\n");

    print("------------------------------------------------------------\n");
}


/*
 * Draw bottom status bar.
 */

static void writer_draw_footer(void)
{
    print("------------------------------------------------------------\n");

    print("Line: ");

    /*
     * TODO:
     * Тут додамо нормальний print_int().
     */

    print("1");

    print("  Column: ");

    print("1");

    print("\n");
}


/*
 * Draw editor contents.
 */

static void writer_draw_editor(void)
{
    unsigned int i = 0;

    while (i < writer_length) {

        put_char(writer_text[i]);

        i++;
    }

    /*
     * Cursor.
     */

    put_char('_');

    update_cursor();
}


/*
 * Insert character at cursor.
 */

static void writer_insert_char(char c)
{
    if (writer_length >= WRITER_MAX_TEXT - 1) {
        return;
    }

    /*
     * Move existing text one position right.
     */

    for (unsigned int i = writer_length;
         i > writer_cursor;
         i--) {

        writer_text[i] =
            writer_text[i - 1];
    }

    writer_text[writer_cursor] = c;

    writer_cursor++;
    writer_length++;
}


/*
 * Backspace.
 */

static void writer_backspace(void)
{
    if (writer_cursor == 0) {
        return;
    }

    for (unsigned int i = writer_cursor - 1;
         i < writer_length - 1;
         i++) {

        writer_text[i] =
            writer_text[i + 1];
    }

    writer_cursor--;
    writer_length--;
}


/*
 * Enter / newline.
 */

static void writer_newline(void)
{
    writer_insert_char('\n');
}


/*
 * Save file.
 *
 * FAT16 implementation will be connected later.
 */

static void writer_save(const char* filename)
{
    print("\n");
    print("FAT16: Saving...\n");

    /*
     * TODO:
     *
     * fat16_write_file(
     *     filename,
     *     writer_text,
     *     writer_length
     * );
     */

    print("FAT16: Save API not connected yet\n");
}


/*
 * Process keyboard input.
 *
 * Зараз це лише базовий рівень.
 * Реальне читання клавіш підключимо
 * до VGA/keyboard driver.
 */

static void writer_process_key(
    unsigned char key,
    const char* filename
)
{
    if (key == WRITER_KEY_ESC) {
        return;
    }

    if (key == WRITER_KEY_F1) {
        writer_save(filename);
        return;
    }

    if (key == '\n') {
        writer_newline();
        return;
    }

    if (key == '\b') {
        writer_backspace();
        return;
    }

    /*
     * Printable ASCII.
     */

    if (key >= 32 && key <= 126) {
        writer_insert_char((char)key);
    }
}


/*
 * Main Writer loop.
 *
 * TODO:
 * Підключити реальний keyboard_get_key().
 */

static void writer_loop(const char* filename)
{
    while (1) {

        /*
         * TODO:
         *
         * unsigned char key =
         *     keyboard_get_key();
         */

        unsigned char key = 0;

        if (key == WRITER_KEY_ESC) {
            break;
        }

        writer_process_key(
            key,
            filename
        );
    }
}


/*
 * Open Writer.
 */

void writer_open(const char* filename)
{
    if (filename == 0 ||
        filename[0] == '\0') {

        print("Usage: write <filename>\n");
        return;
    }

    /*
     * Reset editor.
     */

    writer_length = 0;
    writer_cursor = 0;

    writer_line = 0;
    writer_column = 0;

    writer_scroll = 0;

    /*
     * Clear screen.
     */

    writer_clear_screen();

    /*
     * Header.
     */

    writer_draw_header(filename);

    /*
     * Editor.
     */

    writer_draw_editor();

    /*
     * Footer.
     */

    writer_draw_footer();

    /*
     * Start editor.
     */

    writer_loop(filename);
}