
/*
 * Q-J-R OS Mini Memory Management
 * 1. PMM: Page Frame Allocator (Bitmap 4KB pages)
 * 2. Heap: Simple bump/linked allocator for kmalloc/kfree
 */

#define PAGE_SIZE 4096

// Пам'ять купи ядра почнеться після перших 8 МБ
#define HEAP_START 0x800000
#define HEAP_MAX_SIZE (8 * 1024 * 1024) // 8 MB для kmalloc

// Checking RAM Size constants
#define RAM_CHECK_START 0x100000 // 1 MB (пропускаємо перші 1 МБ BIOS/VGA/IVT)
#define RAM_STEP        0x100000 // Крок 1 MB (1024 * 1024 байт)
#define TEST_MAGIC_1    0xAA55AA55
#define TEST_MAGIC_2    0x55AA55AA

static unsigned int* pmm_bitmap;
static unsigned int  pmm_total_pages = 0;
static unsigned int  pmm_bitmap_size = 0;

/*
 * Повертає кількість доступної оперативної пам'яті в мегабайтах (MB)
 */
unsigned int detect_memory_mb(void) {
    unsigned int total_mb = 1; // 1-й мегабайт уже є за замовчуванням
    unsigned int current_addr = RAM_CHECK_START;

    while (current_addr < 0xFFFFFFFF - RAM_STEP) {
        volatile unsigned int* ptr = (volatile unsigned int*)current_addr;

        // 1. Зберігаємо оригінальне значення, яке там було
        unsigned int original = *ptr;

        // 2. Записуємо тестове значення №1
        *ptr = TEST_MAGIC_1;
        // Скидаємо кеш CPU для цієї адреси (інструкція wbinvd або пряме читання)
        __asm__ volatile ("wbinvd");

        if (*ptr != TEST_MAGIC_1) {
            break; // Якщо значення не збереглося — фізична пам'ять закінчилася
        }

        // 3. Записуємо інвертоване значення №2 (для перевірки залипання шини)
        *ptr = TEST_MAGIC_2;
        __asm__ volatile ("wbinvd");

        if (*ptr != TEST_MAGIC_2) {
            *ptr = original;
            break;
        }

        // 4. Відновлюємо оригінальні дані
        *ptr = original;

        total_mb++;
        current_addr += RAM_STEP;
    }

    return total_mb;
}

// CMOS

static inline unsigned char read_cmos(unsigned char reg) {
    __asm__ volatile ("outb %0, $0x70" : : "a"(reg));
    unsigned char ret;
    __asm__ volatile ("inb $0x71, %0" : "=a"(ret));
    return ret;
}

/*
 * Повертає RAM у кілобайтах через CMOS регістри
 */
unsigned int detect_memory_cmos_kb(void) {
    unsigned short low_mem;
    unsigned short high_mem;

    // Базова пам'ять (зазвичай 640 KB)
    low_mem = read_cmos(0x15) | (read_cmos(0x16) << 8);

    // Розширена пам'ять вище 1 MB (регістри 0x17 та 0x18 або 0x30 та 0x31)
    high_mem = read_cmos(0x30) | (read_cmos(0x31) << 8);

    // 1024 KB (1-й мегабайт) + розширена пам'ять у KB
    return 1024 + high_mem;
}

/*
 * =========================================================================
 * 1. Physical Memory Manager (PMM) — Bitmap 4KB
 * =========================================================================
 */

static inline void pmm_set_bit(unsigned int bit) {
    pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

static inline void pmm_clear_bit(unsigned int bit) {
    pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

static inline int pmm_test_bit(unsigned int bit) {
    return (pmm_bitmap[bit / 32] & (1 << (bit % 32))) != 0;
}

void pmm_init(unsigned int total_ram_mb) {
    unsigned int total_bytes = total_ram_mb * 1024 * 1024;
    pmm_total_pages = total_bytes / PAGE_SIZE;
    pmm_bitmap_size = pmm_total_pages / 8; // у байтах

    // Розміщуємо саму бітову карту за адресою 2 МБ (безпечна зона)
    pmm_bitmap = (unsigned int*)0x200000;

    // Спочатку позначаємо всю пам'ять як зайняту (1)
    for (unsigned int i = 0; i < pmm_bitmap_size / 4; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }

    // Звільняємо (0) доступну пам'ять вище 8 МБ (пропускаємо ядро, буфери й VGA)
    unsigned int free_start_page = 0x800000 / PAGE_SIZE;
    for (unsigned int i = free_start_page; i < pmm_total_pages; i++) {
        pmm_clear_bit(i);
    }
}

// Виділити 1 фізичну сторінку (4 КБ)
void* pmm_alloc_page(void) {
    for (unsigned int i = 0; i < pmm_total_pages; i++) {
        if (!pmm_test_bit(i)) {
            pmm_set_bit(i);
            return (void*)(i * PAGE_SIZE);
        }
    }
    return 0; // Out of memory!
}

// Звільнити фізичну сторінку (4 КБ)
void pmm_free_page(void* addr) {
    unsigned int page = (unsigned int)addr / PAGE_SIZE;
    if (page < pmm_total_pages) {
        pmm_clear_bit(page);
    }
}

/*
 * =========================================================================
 * 2. Heap Allocator (kmalloc / kfree)
 * =========================================================================
 */

// Заголовок кожного виділеного блоку в купі
struct block_header {
    unsigned int size;
    int is_free;
    struct block_header* next;
};

static struct block_header* heap_head = (struct block_header*)HEAP_START;

void kmalloc_init(void) {
    heap_head->size = HEAP_MAX_SIZE - sizeof(struct block_header);
    heap_head->is_free = 1;
    heap_head->next = 0;
}

void* kmalloc(unsigned int size) {
    // Вирівнюємо розмір по 4 байти
    size = (size + 3) & ~3;

    struct block_header* curr = heap_head;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Якщо блок завеликий — ділимо його на два
            if (curr->size >= size + sizeof(struct block_header) + 8) {
                struct block_header* next_block =
                    (struct block_header*)((unsigned int)curr + sizeof(struct block_header) + size);

                next_block->size = curr->size - size - sizeof(struct block_header);
                next_block->is_free = 1;
                next_block->next = curr->next;

                curr->size = size;
                curr->next = next_block;
            }

            curr->is_free = 0;
            // Повертаємо адресу пам'яті одразу за заголовком
            return (void*)((unsigned int)curr + sizeof(struct block_header));
        }
        curr = curr->next;
    }

    return 0; // Немає вільного блоку
}

void kfree(void* ptr) {
    if (!ptr) return;

    struct block_header* header =
        (struct block_header*)((unsigned int)ptr - sizeof(struct block_header));
    header->is_free = 1;

    // Злиття сусідніх вільних блоків (Defragmentation)
    struct block_header* curr = heap_head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(struct block_header) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}