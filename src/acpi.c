#include "include/io.h"
#include "include/stdint.h"

// Заголовок RSDP (Root System Description Pointer)
struct rsdp_descriptor {
    char signature[8]; // "RSD PTR "
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

// Базовий заголовок будь-якої ACPI-таблиці
struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

// Таблиця FADT (Fixed ACPI Description Table)
struct fadt_table {
    struct acpi_sdt_header header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t  reserved;
    uint8_t  preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block; // Базовий I/O порт керування живленням
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    // Інші поля поки не потрібні
} __attribute__((packed));

static uint32_t pm1a_cnt = 0;
static uint16_t  slp_typa = (5 << 10); // За замовчуванням стан сну S5 = 5
static uint16_t slp_en   = (1 << 13); // Біт ініціалізації Sleep Enable

static int acpi_memcmp(const void* s1, const void* s2, uint32_t n) {
    const uint8_t *p1 = s1, *p2 = s2;
    for (uint32_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

// Пошук RSDP в пам'яті BIOS (0xE0000 - 0xFFFFF)
static struct rsdp_descriptor* acpi_find_rsdp(void) {
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        if (acpi_memcmp((void*)addr, "RSD PTR ", 8) == 0) {
            return (struct rsdp_descriptor*)addr;
        }
    }
    return (void*)0;
}

// Ініціалізація та пошук FADT
int acpi_init(void) {
    struct rsdp_descriptor* rsdp = acpi_find_rsdp();
    if (!rsdp) return -1; // ACPI не підтримується цим залізом

    struct acpi_sdt_header* rsdt = (struct acpi_sdt_header*)rsdp->rsdt_address;
    if (!rsdt || acpi_memcmp(rsdt->signature, "RSDT", 4) != 0) return -2;

    uint32_t entries = (rsdt->length - sizeof(struct acpi_sdt_header)) / 4;
    uint32_t* table_ptrs = (uint32_t*)((uint32_t)rsdt + sizeof(struct acpi_sdt_header));

    for (uint32_t i = 0; i < entries; i++) {
        struct acpi_sdt_header* h = (struct acpi_sdt_header*)table_ptrs[i];
        if (acpi_memcmp(h->signature, "FACP", 4) == 0) {
            struct fadt_table* fadt = (struct fadt_table*)h;
            pm1a_cnt = fadt->pm1a_control_block;

            // Якщо ACPI ще не активований контролером BIOS, надсилаємо сигнал увімкнення
            if (fadt->smi_command_port && fadt->acpi_enable) {
                outb(fadt->smi_command_port, fadt->acpi_enable);
                // Даємо час материнській платі перемкнутися в ACPI-режим
                for (volatile int w = 0; w < 100000; w++) io_wait();
            }
            return 0;
        }
    }

    return -3; // Таблицю FADT не знайдено
}

void acpi_power_off(void) {
    if (pm1a_cnt != 0) {
        // Sending a command for entry into S5 status
        outw((uint16_t)pm1a_cnt, slp_typa | slp_en);

        // If there is an additional PM1b control unit (found on some boards)
        // outw((uint16_t)pm1b_cnt, slp_typb | slp_en);

        // A brief pause to allow the chipset time to process the power-off.
        for (volatile int i = 0; i < 500000; i++) {
            io_wait();
        }
    }
    // We do NOT put a `while(1) hlt` here; we allow the backup ports to operate!
}