
#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7

#define ATA_CMD_IDENTIFY      0xEC
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30

#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRDY 0x40
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_ERR  0x01

#define ATA_DRIVE_MASTER 0xE0
#define ATA_DRIVE_SLAVE  0xF0

#define FAT16_CLUSTER_FREE 0x0000
#define FAT16_CLUSTER_EOF  0xFFFF

struct fat16_bpb {
    unsigned char jump[3];
    unsigned char oem[8];
    unsigned short bytes_per_sector;
    unsigned char sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char fat_count;
    unsigned short root_entries;
    unsigned short total_sectors;
    unsigned char media;
    unsigned short sectors_per_fat;
    unsigned short sectors_per_track;
    unsigned short heads;
    unsigned int hidden_sectors;
    unsigned int large_total_sectors;
};

struct fat16_info {
    unsigned short bytes_per_sector;
    unsigned char sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char fat_count;
    unsigned short root_entries;
    unsigned int total_sectors;
    unsigned short sectors_per_fat;
};

static struct fat16_info fat16;

// extern char input[64];

extern int strcmp(const char* a, const char* b);

extern void put_char(char c);
extern void print(const char* str);
extern void update_cursor(void);
extern void input_text(const char* prompt, char* buffer, int buffer_size);
extern char* cut_text(const char* text, int index_start, int index_end);
extern int get_first_space_index(const char* text);
extern char* get_arg(const char* text);

// 0 = Root directory, >= 2 = Cluster of current directory
static unsigned short current_dir_cluster = 0;
static char current_path[128] = "/";

// ATA INB
static unsigned char ata_inb(unsigned short port)
{
    unsigned char value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

// ATA OUTB

static void ata_outb(unsigned short port, unsigned char value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

// ATA OUTW

static void ata_outw(
    unsigned short port,
    unsigned short value
)
{
    __asm__ volatile (
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

// INW

static unsigned short ata_inw(unsigned short port)
{
    unsigned short value;

    __asm__ volatile (
        "inw %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

// ATA wait
static int ata_wait_bsy(void)
{
    unsigned char status;

    for (unsigned int i = 0; i < 1000000; i++) {
        status = ata_inb(ATA_STATUS);
        if (!(status & ATA_STATUS_BSY))
            return status;
    }

    return 0x80;
}


static int ata_wait_drq(void)
{
    unsigned char status;

    for (unsigned int i = 0; i < 1000000; i++) {
        status = ata_inb(ATA_STATUS);

        if (status & ATA_STATUS_ERR)
            return 0;

        if (status & ATA_STATUS_DRQ)
            return 1;
    }

    return 0;
}

// ATA: Read Sector
int ata_read_sector(unsigned char drive, unsigned int lba, unsigned char* buffer)
{
    ata_wait_bsy();

    // ata_outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    ata_outb(ATA_DRIVE, drive | ((lba >> 24) & 0x0F));

    ata_outb(ATA_SECTOR_CNT, 1);
    ata_outb(ATA_LBA_LOW,  lba & 0xFF);
    ata_outb(ATA_LBA_MID,  (lba >> 8) & 0xFF);
    ata_outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);

    ata_outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    if (!ata_wait_drq())
        return 0;

    for (int i = 0; i < 256; i++) {
        unsigned short word = ata_inw(ATA_DATA);

        buffer[i * 2]     = word & 0xFF;
        buffer[i * 2 + 1] = word >> 8;
    }

    ata_wait_bsy();

    return 1;
}

// ATA: Write Sector

int ata_write_sector(
    unsigned char drive,
    unsigned int lba,
    const unsigned char* buffer
)
{
    ata_wait_bsy();

    ata_outb(
        ATA_DRIVE,
        drive | ((lba >> 24) & 0x0F)
    );

    ata_outb(ATA_SECTOR_CNT, 1);

    ata_outb(
        ATA_LBA_LOW,
        lba & 0xFF
    );

    ata_outb(
        ATA_LBA_MID,
        (lba >> 8) & 0xFF
    );

    ata_outb(
        ATA_LBA_HIGH,
        (lba >> 16) & 0xFF
    );

    ata_outb(
        ATA_COMMAND,
        ATA_CMD_WRITE_SECTORS
    );

    if (!ata_wait_drq())
        return 0;

    for (int i = 0; i < 256; i++) {
        unsigned short word =
            buffer[i * 2] |
            ((unsigned short)buffer[i * 2 + 1] << 8);

        ata_outw(ATA_DATA, word);
    }

    ata_wait_bsy();

    return 1;
}

// little-endian

static unsigned short fat16_read_u16(const unsigned char* data)
{
    return data[0] | ((unsigned short)data[1] << 8);
}

static unsigned short fat16_get_next_cluster(
    unsigned short cluster
)
{
    unsigned char buffer[512];

    unsigned int fat_offset =
        (unsigned int)cluster * 2;

    unsigned int sector =
        fat16.reserved_sectors +
        (fat_offset / fat16.bytes_per_sector);

    unsigned int offset =
        fat_offset % fat16.bytes_per_sector;

    if (!ata_read_sector(
            ATA_DRIVE_SLAVE,
            sector,
            buffer)) {

        print("FAT16: FAT read error\n");
        return 0;
    }

    return fat16_read_u16(&buffer[offset]);
}


static unsigned short fat16_find_free_cluster(void)
{
    unsigned char buffer[512];

    unsigned int fat_start =
        fat16.reserved_sectors;

    unsigned int max_cluster =
        2 + (
            (unsigned int)fat16.total_sectors -
            (
                fat16.reserved_sectors +
                ((unsigned int)fat16.fat_count *
                 fat16.sectors_per_fat) +
                (
                    ((unsigned int)fat16.root_entries * 32 +
                     fat16.bytes_per_sector - 1) /
                    fat16.bytes_per_sector
                )
            )
        ) / fat16.sectors_per_cluster;

    for (unsigned int cluster = 2;
         cluster < max_cluster;
         cluster++) {

        unsigned int fat_offset =
            cluster * 2;

        unsigned int sector =
            fat_start +
            (fat_offset / fat16.bytes_per_sector);

        unsigned int offset =
            fat_offset % fat16.bytes_per_sector;

        if (!ata_read_sector(
                ATA_DRIVE_SLAVE,
                sector,
                buffer)) {

            print("FAT16: FAT read error\n");
            return 0;
                }

        unsigned short value =
            fat16_read_u16(&buffer[offset]);

        if (value == FAT16_CLUSTER_FREE)
            return cluster;
         }

    return 0;
}
static int fat16_set_cluster(
    unsigned short cluster,
    unsigned short value
)
{
    unsigned char buffer[512];

    unsigned int fat_offset =
        cluster * 2;

    unsigned int fat_sector =
        fat_offset / fat16.bytes_per_sector;

    unsigned int offset =
        fat_offset % fat16.bytes_per_sector;

    for (unsigned int fat = 0;
         fat < fat16.fat_count;
         fat++) {

        unsigned int sector =
            fat16.reserved_sectors +
            (fat * fat16.sectors_per_fat) +
            fat_sector;

        if (!ata_read_sector(
                ATA_DRIVE_SLAVE,
                sector,
                buffer)) {

            print("FAT16: FAT read error\n");
            return 0;
        }

        buffer[offset] =
            value & 0xFF;

        buffer[offset + 1] =
            (value >> 8) & 0xFF;

        if (!ata_write_sector(
                ATA_DRIVE_SLAVE,
                sector,
                buffer)) {

            print("FAT16: FAT write error\n");
            return 0;
        }
    }

    return 1;
}


static unsigned short fat16_allocate_cluster_chain(
    unsigned int cluster_count
)
{
    unsigned short first_cluster = 0;
    unsigned short previous_cluster = 0;

    for (unsigned int i = 0; i < cluster_count; i++) {

        unsigned short cluster =
            fat16_find_free_cluster();

        if (cluster == 0) {
            print("FAT16: No free clusters\n");

            if (first_cluster != 0) {
                unsigned short current = first_cluster;

                while (current >= 2 &&
                       current < 0xFFF8) {

                    unsigned short next =
                        fat16_get_next_cluster(current);

                    fat16_set_cluster(
                        current,
                        FAT16_CLUSTER_FREE
                    );

                    if (next >= 0xFFF8)
                        break;

                    current = next;
                }
            }

            return 0;
        }

        if (previous_cluster != 0) {

            if (!fat16_set_cluster(
                    previous_cluster,
                    cluster)) {

                print("FAT16: Failed to link clusters\n");
                return 0;
            }
        }

        if (!fat16_set_cluster(
                cluster,
                FAT16_CLUSTER_EOF)) {

            print("FAT16: Failed to allocate cluster\n");
            return 0;
        }

        if (first_cluster == 0)
            first_cluster = cluster;

        previous_cluster = cluster;
    }

    return first_cluster;
}

// crete root entry

static int fat16_create_root_entry(
    const char* filename,
    unsigned short cluster,
    unsigned int file_size
)
{
    unsigned char buffer[512];

    unsigned int root_dir_start =
        fat16.reserved_sectors +
        ((unsigned int)fat16.fat_count * fat16.sectors_per_fat);

    unsigned int root_dir_sectors =
        ((unsigned int)fat16.root_entries * 32 +
         fat16.bytes_per_sector - 1) /
        fat16.bytes_per_sector;

    char name[8];
    char ext[3];

    for (int i = 0; i < 8; i++)
        name[i] = ' ';

    for (int i = 0; i < 3; i++)
        ext[i] = ' ';

    int i = 0;
    int j = 0;

    while (filename[i] != '\0' &&
           filename[i] != '.' &&
           i < 8) {

        char c = filename[i];

        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';

        name[i] = c;
        i++;
    }

    if (filename[i] == '.') {
        i++;

        while (filename[i] != '\0' && j < 3) {
            char c = filename[i];

            if (c >= 'a' && c <= 'z')
                c -= 'a' - 'A';

            ext[j++] = c;
            i++;
        }
    }

    for (unsigned int sector = 0;
         sector < root_dir_sectors;
         sector++) {

        if (!ata_read_sector(ATA_DRIVE_SLAVE, root_dir_start + sector, buffer)) {
            print("FAT16: Directory read error\n");
            return 0;
        }

        for (unsigned int offset = 0;
             offset < fat16.bytes_per_sector;
             offset += 32) {

            unsigned char* entry = &buffer[offset];

            if (entry[0] != 0x00 &&
                entry[0] != 0xE5)
                continue;

            for (int k = 0; k < 8; k++)
                entry[k] = name[k];

            for (int k = 0; k < 3; k++)
                entry[8 + k] = ext[k];

            entry[11] = 0x20;

            for (int k = 12; k < 26; k++)
                entry[k] = 0;

            entry[26] = cluster & 0xFF;
            entry[27] = (cluster >> 8) & 0xFF;

            entry[28] = file_size & 0xFF;
            entry[29] = (file_size >> 8) & 0xFF;
            entry[30] = (file_size >> 16) & 0xFF;
            entry[31] = (file_size >> 24) & 0xFF;

            if (!ata_write_sector(
                    ATA_DRIVE_SLAVE,
                    root_dir_start + sector,
                    buffer)) {

                print("FAT16: Directory write error\n");
                return 0;
            }

            return 1;
        }
    }

    print("FAT16: Root directory full\n");
    return 0;
}


static unsigned int fat16_read_u32(const unsigned char* data)
{
    return data[0]
        | ((unsigned int)data[1] << 8)
        | ((unsigned int)data[2] << 16)
        | ((unsigned int)data[3] << 24);
}



// Validate FAT16 BPB

static int fat16_validate_bpb(const unsigned char* buffer) {
    unsigned short bytes_per_sector = fat16_read_u16(&buffer[0x0B]);
    unsigned char sectors_per_cluster = buffer[0x0D];
    unsigned short reserved_sectors = fat16_read_u16(&buffer[0x0E]);
    unsigned char fat_count = buffer[0x10];
    unsigned short root_entries = fat16_read_u16(&buffer[0x11]);
    unsigned short total_sectors = fat16_read_u16(&buffer[0x13]);
    unsigned short sectors_per_fat = fat16_read_u16(&buffer[0x16]);

    /*
     * FAT16 normally uses
     * 512-byte sectors.
     */

    if (bytes_per_sector != 512)
        return 0;

    if (sectors_per_cluster == 0)
        return 0;

    if (reserved_sectors == 0)
        return 0;

    if (fat_count == 0)
        return 0;

    if (root_entries == 0)
        return 0;

    if (total_sectors == 0)
        return 0;

    if (sectors_per_fat == 0)
        return 0;

    /*
     * FAT16 boot sector signature.
     */

    if (buffer[510] != 0x55 ||
        buffer[511] != 0xAA)
    {
        return 0;
    }
    return 1;
}

// create_root_directory_entry

static int fat16_create_root_dir_entry(
    const char* dirname,
    unsigned short cluster
)
{
    unsigned char buffer[512];

    unsigned int root_dir_start =
        fat16.reserved_sectors +
        ((unsigned int)fat16.fat_count * fat16.sectors_per_fat);

    unsigned int root_dir_sectors =
        ((unsigned int)fat16.root_entries * 32 +
         fat16.bytes_per_sector - 1) /
        fat16.bytes_per_sector;

    char name[8];
    char ext[3];

    for (int i = 0; i < 8; i++)
        name[i] = ' ';

    for (int i = 0; i < 3; i++)
        ext[i] = ' ';

    int i = 0;

    // Папки зазвичай не мають розширень (беруться перші до 8 символів)
    while (dirname[i] != '\0' && dirname[i] != '.' && i < 8) {
        char c = dirname[i];
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        name[i] = c;
        i++;
    }

    for (unsigned int sector = 0; sector < root_dir_sectors; sector++) {
        if (!ata_read_sector(ATA_DRIVE_SLAVE, root_dir_start + sector, buffer)) {
            print("FAT16: Directory read error\n");
            return 0;
        }

        for (unsigned int offset = 0; offset < fat16.bytes_per_sector; offset += 32) {
            unsigned char* entry = &buffer[offset];

            if (entry[0] != 0x00 && entry[0] != 0xE5)
                continue;

            for (int k = 0; k < 8; k++)
                entry[k] = name[k];

            for (int k = 0; k < 3; k++)
                entry[8 + k] = ext[k];

            // 0x10 = Атрибут директорії (Directory Attribute)
            entry[11] = 0x10;

            for (int k = 12; k < 26; k++)
                entry[k] = 0;

            entry[26] = cluster & 0xFF;
            entry[27] = (cluster >> 8) & 0xFF;

            // Розмір директорії в FAT16 завжди 0
            entry[28] = 0;
            entry[29] = 0;
            entry[30] = 0;
            entry[31] = 0;

            if (!ata_write_sector(ATA_DRIVE_SLAVE, root_dir_start + sector, buffer)) {
                print("FAT16: Directory write error\n");
                return 0;
            }

            return 1;
        }
    }

    print("FAT16: Root directory is full\n");
    return 0;
}

static int fat16_add_entry_to_current_dir(
    const char* filename,
    unsigned short cluster,
    unsigned int file_size,
    unsigned char attr
)
{
    unsigned char buffer[512];
    char name[8];
    char ext[3];

    for (int i = 0; i < 8; i++) name[i] = ' ';
    for (int i = 0; i < 3; i++) ext[i] = ' ';

    int i = 0;
    int j = 0;

    while (filename[i] != '\0' && filename[i] != '.' && i < 8) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        name[i] = c;
        i++;
    }

    if (filename[i] == '.') {
        i++;
        while (filename[i] != '\0' && j < 3) {
            char c = filename[i];
            if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
            ext[j++] = c;
            i++;
        }
    }

    // 1. Якщо ми в Root Directory (current_dir_cluster == 0)
    if (current_dir_cluster == 0) {
        unsigned int root_dir_start =
            fat16.reserved_sectors +
            ((unsigned int)fat16.fat_count * fat16.sectors_per_fat);

        unsigned int root_dir_sectors =
            ((unsigned int)fat16.root_entries * 32 +
             fat16.bytes_per_sector - 1) /
            fat16.bytes_per_sector;

        for (unsigned int sector = 0; sector < root_dir_sectors; sector++) {
            if (!ata_read_sector(ATA_DRIVE_SLAVE, root_dir_start + sector, buffer)) {
                print("FAT16: Directory read error\n");
                return 0;
            }

            for (unsigned int offset = 0; offset < fat16.bytes_per_sector; offset += 32) {
                unsigned char* entry = &buffer[offset];

                if (entry[0] != 0x00 && entry[0] != 0xE5)
                    continue;

                for (int k = 0; k < 8; k++) entry[k] = name[k];
                for (int k = 0; k < 3; k++) entry[8 + k] = ext[k];

                entry[11] = attr;

                for (int k = 12; k < 26; k++) entry[k] = 0;

                entry[26] = cluster & 0xFF;
                entry[27] = (cluster >> 8) & 0xFF;

                entry[28] = file_size & 0xFF;
                entry[29] = (file_size >> 8) & 0xFF;
                entry[30] = (file_size >> 16) & 0xFF;
                entry[31] = (file_size >> 24) & 0xFF;

                if (!ata_write_sector(ATA_DRIVE_SLAVE, root_dir_start + sector, buffer)) {
                    print("FAT16: Directory write error\n");
                    return 0;
                }
                return 1;
            }
        }
        print("FAT16: Root directory full\n");
        return 0;
    }

    // 2. Якщо ми в Subdirectory (current_dir_cluster >= 2)
    unsigned int data_start =
        fat16.reserved_sectors +
        ((unsigned int)fat16.fat_count * fat16.sectors_per_fat) +
        (((unsigned int)fat16.root_entries * 32 +
          fat16.bytes_per_sector - 1) /
         fat16.bytes_per_sector);

    unsigned short curr = current_dir_cluster;
    unsigned short last_cluster = curr;

    while (curr >= 2 && curr < 0xFFF8) {
        last_cluster = curr;
        unsigned int cluster_sector =
            data_start + ((unsigned int)(curr - 2) * fat16.sectors_per_cluster);

        for (unsigned int s = 0; s < fat16.sectors_per_cluster; s++) {
            if (!ata_read_sector(ATA_DRIVE_SLAVE, cluster_sector + s, buffer)) {
                return 0;
            }

            for (unsigned int offset = 0; offset < fat16.bytes_per_sector; offset += 32) {
                unsigned char* entry = &buffer[offset];

                if (entry[0] != 0x00 && entry[0] != 0xE5)
                    continue;

                for (int k = 0; k < 8; k++) entry[k] = name[k];
                for (int k = 0; k < 3; k++) entry[8 + k] = ext[k];

                entry[11] = attr;

                for (int k = 12; k < 26; k++) entry[k] = 0;

                entry[26] = cluster & 0xFF;
                entry[27] = (cluster >> 8) & 0xFF;

                entry[28] = file_size & 0xFF;
                entry[29] = (file_size >> 8) & 0xFF;
                entry[30] = (file_size >> 16) & 0xFF;
                entry[31] = (file_size >> 24) & 0xFF;

                if (!ata_write_sector(ATA_DRIVE_SLAVE, cluster_sector + s, buffer)) {
                    return 0;
                }
                return 1;
            }
        }
        curr = fat16_get_next_cluster(curr);
    }

    // Якщо вільних слотів не вистачило — виділяємо новий кластер для піддиректорії
    unsigned short new_dir_cluster = fat16_allocate_cluster_chain(1);
    if (new_dir_cluster == 0) {
        print("FAT16: Subdirectory full, allocation failed\n");
        return 0;
    }

    if (!fat16_set_cluster(last_cluster, new_dir_cluster)) {
        return 0;
    }

    // Зануляємо новий кластер
    unsigned int new_sector =
        data_start + ((unsigned int)(new_dir_cluster - 2) * fat16.sectors_per_cluster);

    for (int k = 0; k < 512; k++) buffer[k] = 0;

    // Перший запис — наш новий файл/папка
    for (int k = 0; k < 8; k++) buffer[k] = name[k];
    for (int k = 0; k < 3; k++) buffer[8 + k] = ext[k];
    buffer[11] = attr;
    buffer[26] = cluster & 0xFF;
    buffer[27] = (cluster >> 8) & 0xFF;
    buffer[28] = file_size & 0xFF;
    buffer[29] = (file_size >> 8) & 0xFF;
    buffer[30] = (file_size >> 16) & 0xFF;
    buffer[31] = (file_size >> 24) & 0xFF;

    ata_write_sector(ATA_DRIVE_SLAVE, new_sector, buffer);

    for (int k = 0; k < 512; k++) buffer[k] = 0;
    for (unsigned int s = 1; s < fat16.sectors_per_cluster; s++) {
        ata_write_sector(ATA_DRIVE_SLAVE, new_sector + s, buffer);
    }

    return 1;
}


// command: ata
void init_ata(void) {
    // something ...
    // mount FAT16 drive via mkfs.fat 4.2 on GNU/Linux / macOS
    // initialize ATA
    // if FAT16 disk is fully available and ATA initialized => print("ATA: Drive Found"),
    // else: print("ATA: Disk Error")

    unsigned char buffer[512];

    print("ATA: Checking drive...\n");

    if (!ata_read_sector(
            ATA_DRIVE_SLAVE,
            0,
            buffer)) {

        print("ATA: Disk Error\n");
        return;

    }

    print("ATA: Drive Found\n");

    /*
     * Check FAT16 BPB.
     */

    if (!fat16_validate_bpb(buffer)) {
        print("FAT16: Invalid BPB\n");
        return;
    }

    /*
     * Read BPB values.
     */

    fat16.bytes_per_sector = fat16_read_u16(&buffer[0x0B]);
    fat16.sectors_per_cluster = buffer[0x0D];
    fat16.reserved_sectors = fat16_read_u16(&buffer[0x0E]);
    fat16.fat_count = buffer[0x10];
    fat16.root_entries = fat16_read_u16(&buffer[0x11]);
    // fat16.total_sectors = fat16_read_u16(&buffer[0x13]);
    fat16.total_sectors = fat16_read_u16(&buffer[0x13]);

    if (fat16.total_sectors == 0) {
        fat16.total_sectors = fat16_read_u32(&buffer[0x20]);
    }

    fat16.sectors_per_fat = fat16_read_u16(&buffer[0x16]);

    print("FAT16: Mounted\n");

    // unsigned short free_cluster = fat16_find_free_cluster();

    // if (free_cluster == 0) {
    //    print("FAT16: No free clusters\n");
    //    return;
    // }

    // print("FAT16: Free cluster found\n");


    // if (!fat16_set_cluster(free_cluster, FAT16_CLUSTER_EOF)) {
    //    print("FAT16: Failed to allocate cluster\n");
    //    return;

    // }

    // print("FAT16: Cluster allocated\n");
}

// command: ls

void ata_ls(void) {
    if (fat16.bytes_per_sector == 0) {
        print("FAT16: Not mounted\n");
        return;
    }

    unsigned char buffer[512];
    print("Files:\n");

    // 1. Якщо ми в Root
    if (current_dir_cluster == 0) {
        unsigned int root_dir_start =
            fat16.reserved_sectors + ((unsigned int)fat16.fat_count * fat16.sectors_per_fat);
        unsigned int root_dir_sectors =
            ((unsigned int)fat16.root_entries * 32 + fat16.bytes_per_sector - 1) / fat16.bytes_per_sector;

        for (unsigned int sector = 0; sector < root_dir_sectors; sector++) {
            if (!ata_read_sector(ATA_DRIVE_SLAVE, root_dir_start + sector, buffer)) {
                print("FAT16: Directory read error\n");
                return;
            }

            for (unsigned int offset = 0; offset < fat16.bytes_per_sector; offset += 32) {
                unsigned char* entry = &buffer[offset];
                if (entry[0] == 0x00) return;
                if (entry[0] == 0xE5 || entry[11] == 0x0F || (entry[11] & 0x08)) continue;

                if (entry[11] & 0x10) print("[DIR]  ");
                else                  print("[FILE] ");

                for (int i = 0; i < 8; i++) {
                    if (entry[i] == ' ') break;
                    put_char(entry[i]);
                }
                if (entry[8] != ' ') {
                    put_char('.');
                    for (int i = 8; i < 11; i++) {
                        if (entry[i] == ' ') break;
                        put_char(entry[i]);
                    }
                }
                print("\n");
            }
        }
    }
    // 2. Якщо ми в підпапці
    else {
        unsigned int data_start =
            fat16.reserved_sectors +
            ((unsigned int)fat16.fat_count * fat16.sectors_per_fat) +
            (((unsigned int)fat16.root_entries * 32 + fat16.bytes_per_sector - 1) / fat16.bytes_per_sector);

        unsigned short cluster = current_dir_cluster;

        while (cluster >= 2 && cluster < 0xFFF8) {
            unsigned int cluster_sector =
                data_start + ((unsigned int)(cluster - 2) * fat16.sectors_per_cluster);

            for (unsigned int s = 0; s < fat16.sectors_per_cluster; s++) {
                if (!ata_read_sector(ATA_DRIVE_SLAVE, cluster_sector + s, buffer)) {
                    print("FAT16: Directory read error\n");
                    return;
                }

                for (unsigned int offset = 0; offset < fat16.bytes_per_sector; offset += 32) {
                    unsigned char* entry = &buffer[offset];
                    if (entry[0] == 0x00) return;
                    if (entry[0] == 0xE5 || entry[11] == 0x0F) continue;

                    if (entry[11] & 0x10) print("[DIR]  ");
                    else                  print("[FILE] ");

                    for (int i = 0; i < 8; i++) {
                        if (entry[i] == ' ') break;
                        put_char(entry[i]);
                    }
                    if (entry[8] != ' ') {
                        put_char('.');
                        for (int i = 8; i < 11; i++) {
                            if (entry[i] == ' ') break;
                            put_char(entry[i]);
                        }
                    }
                    print("\n");
                }
            }
            cluster = fat16_get_next_cluster(cluster);
        }
    }
}

//void ata_ls(void) { // maybe path in args, don't know
//    // print current directory / path files list
//    if (fat16.bytes_per_sector == 0) {
//        print("FAT16: Not mounted\n");
//        return;
//    }

//    unsigned int root_dir_start =
//        fat16.reserved_sectors +
//        ((unsigned int)fat16.fat_count * fat16.sectors_per_fat);

//    unsigned int root_dir_sectors =
//        ((unsigned int)fat16.root_entries * 32 +
//         fat16.bytes_per_sector - 1) /
//        fat16.bytes_per_sector;

//    unsigned char buffer[512];

//    print("Files:\n");

//    for (unsigned int sector = 0;
//         sector < root_dir_sectors;
//         sector++) {

//        if (!ata_read_sector(
//                ATA_DRIVE_SLAVE,
//                root_dir_start + sector,
//                buffer)) {

//            print("FAT16: Directory read error\n");
//            return;
//        }

//        for (unsigned int offset = 0;
//             offset < fat16.bytes_per_sector;
//             offset += 32) {

//            unsigned char* entry = &buffer[offset];

            // End of directory
//            if (entry[0] == 0x00)
//                return;

            // Deleted entry
//            if (entry[0] == 0xE5)
//                continue;

            // Long File Name entry
//            if (entry[11] == 0x0F)
//                continue;

            // Volume label
//            if (entry[11] & 0x08)
//                continue;

            // If it's a directory

//            if (entry[11] & 0x10) {
//                print("[DIR]  ");
//            } else {
//                print("[FILE] ");
//            }

            // Filename: 8 characters
//            for (int i = 0; i < 8; i++) {
//                if (entry[i] == ' ')
//                    break;

//                put_char(entry[i]);
//            }

            // Extension
//            if (entry[8] != ' ') {
//                put_char('.');

//                for (int i = 8; i < 11; i++) {
//                    if (entry[i] == ' ')
//                        break;

//                    put_char(entry[i]);
//                }
//            }
//
//            print("\n");
//             }
//         }
//}

// ATA: read <filename>

void ata_read(char* filename)
{
    if (fat16.bytes_per_sector == 0) {
        print("FAT16: Not mounted\n");
        return;
    }

    if (filename == 0 || filename[0] == '\0') {
        print("Usage: read <filename>\n");
        return;
    }

    unsigned int root_dir_start =
        fat16.reserved_sectors +
        ((unsigned int)fat16.fat_count *
         fat16.sectors_per_fat);

    unsigned int root_dir_sectors =
        ((unsigned int)fat16.root_entries * 32 +
         fat16.bytes_per_sector - 1) /
        fat16.bytes_per_sector;

    unsigned char buffer[512];

    unsigned short file_cluster = 0;
    unsigned int file_size = 0;

    char name[8];
    char ext[3];

    for (int i = 0; i < 8; i++)
        name[i] = ' ';

    for (int i = 0; i < 3; i++)
        ext[i] = ' ';

    int i = 0;
    int j = 0;

    while (filename[i] != '\0' &&
           filename[i] != '.' &&
           i < 8) {

        char c = filename[i];

        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';

        name[i] = c;
        i++;
    }

    if (filename[i] == '.') {
        i++;

        while (filename[i] != '\0' && j < 3) {
            char c = filename[i];

            if (c >= 'a' && c <= 'z')
                c -= 'a' - 'A';

            ext[j++] = c;
            i++;
        }
    }

    /*
     * Search root directory.
     */

    for (unsigned int sector = 0;
         sector < root_dir_sectors;
         sector++) {

        if (!ata_read_sector(
                ATA_DRIVE_SLAVE,
                root_dir_start + sector,
                buffer)) {

            print("FAT16: Directory read error\n");
            return;
        }

        for (unsigned int offset = 0;
             offset < fat16.bytes_per_sector;
             offset += 32) {

            unsigned char* entry = &buffer[offset];

            if (entry[0] == 0x00)
                break;

            if (entry[0] == 0xE5)
                continue;

            if (entry[11] == 0x0F)
                continue;

            if (entry[11] & 0x08)
                continue;

            int match = 1;

            for (int k = 0; k < 8; k++) {
                if (entry[k] != (unsigned char)name[k]) {
                    match = 0;
                    break;
                }
            }

            if (match) {
                for (int k = 0; k < 3; k++) {
                    if (entry[8 + k] !=
                        (unsigned char)ext[k]) {

                        match = 0;
                        break;
                    }
                }
            }

            if (!match)
                continue;

            file_cluster =
                entry[26] |
                ((unsigned short)entry[27] << 8);

            file_size =
                entry[28] |
                ((unsigned int)entry[29] << 8) |
                ((unsigned int)entry[30] << 16) |
                ((unsigned int)entry[31] << 24);

            break;
        }

        if (file_cluster != 0)
            break;
    }

    if (file_cluster == 0) {
        print("FAT16: File not found\n");
        return;
    }

    print("FAT16: File found\n");

    /*
     * Calculate beginning of data area.
     */

    unsigned int data_start =
        fat16.reserved_sectors +
        ((unsigned int)fat16.fat_count *
         fat16.sectors_per_fat) +
        (((unsigned int)fat16.root_entries * 32 +
          fat16.bytes_per_sector - 1) /
         fat16.bytes_per_sector);

    unsigned int first_sector =
        data_start +
        ((unsigned int)(file_cluster - 2) *
         fat16.sectors_per_cluster);

    unsigned char data[512];

    unsigned int remaining = file_size;

/*
 * Read FAT16 cluster chain.
 */

unsigned short current_cluster = file_cluster;

while (current_cluster >= 2 &&
       current_cluster < 0xFFF8 &&
       remaining > 0) {

    unsigned int cluster_sector =
        data_start +
        ((unsigned int)(current_cluster - 2) *
         fat16.sectors_per_cluster);

    for (unsigned int sector = 0;
         sector < fat16.sectors_per_cluster &&
         remaining > 0;
         sector++) {

        if (!ata_read_sector(
                ATA_DRIVE_SLAVE,
                cluster_sector + sector,
                data)) {

            print("FAT16: Data read error\n");
            return;
        }

        unsigned int bytes_to_print = 512;

        if (remaining < bytes_to_print)
            bytes_to_print = remaining;

        for (unsigned int k = 0;
             k < bytes_to_print;
             k++) {

            put_char(data[k]);
        }

        remaining -= bytes_to_print;
    }

    if (remaining == 0)
        break;

    current_cluster =
        fat16_get_next_cluster(current_cluster);

    if (current_cluster == 0) {
        print("\nFAT16: Invalid cluster chain\n");
        return;
    }
}

print("\n");
}

// command: stat <filename>
void ata_stat(char* filename)
{
    if (fat16.bytes_per_sector == 0) {
        print("FAT16: Not mounted\n");
        return;
    }

    if (filename == 0 || filename[0] == '\0') {
        print("Usage: stat <filename>\n");
        return;
    }

    unsigned int root_dir_start =
        fat16.reserved_sectors +
        ((unsigned int)fat16.fat_count *
         fat16.sectors_per_fat);

    unsigned int root_dir_sectors =
        ((unsigned int)fat16.root_entries * 32 +
         fat16.bytes_per_sector - 1) /
        fat16.bytes_per_sector;

    unsigned char buffer[512];

    unsigned short file_cluster = 0;
    unsigned int file_size = 0;

    char name[8];
    char ext[3];

    for (int i = 0; i < 8; i++)
        name[i] = ' ';

    for (int i = 0; i < 3; i++)
        ext[i] = ' ';

    int i = 0;
    int j = 0;

    while (filename[i] != '\0' &&
           filename[i] != '.' &&
           i < 8) {

        char c = filename[i];

        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';

        name[i] = c;
        i++;
    }

    if (filename[i] == '.') {
        i++;

        while (filename[i] != '\0' && j < 3) {
            char c = filename[i];

            if (c >= 'a' && c <= 'z')
                c -= 'a' - 'A';

            ext[j++] = c;
            i++;
        }
    }

    /*
     * Search root directory.
     */

    for (unsigned int sector = 0;
         sector < root_dir_sectors;
         sector++) {

        if (!ata_read_sector(
                ATA_DRIVE_SLAVE,
                root_dir_start + sector,
                buffer)) {

            print("FAT16: Directory read error\n");
            return;
        }

        for (unsigned int offset = 0;
             offset < fat16.bytes_per_sector;
             offset += 32) {

            unsigned char* entry = &buffer[offset];

            if (entry[0] == 0x00)
                break;

            if (entry[0] == 0xE5)
                continue;

            if (entry[11] == 0x0F)
                continue;

            if (entry[11] & 0x08)
                continue;

            int match = 1;

            for (int k = 0; k < 8; k++) {
                if (entry[k] != (unsigned char)name[k]) {
                    match = 0;
                    break;
                }
            }

            if (match) {
                for (int k = 0; k < 3; k++) {
                    if (entry[8 + k] !=
                        (unsigned char)ext[k]) {

                        match = 0;
                        break;
                    }
                }
            }

            if (!match)
                continue;

            file_cluster =
                entry[26] |
                ((unsigned short)entry[27] << 8);

            file_size =
                entry[28] |
                ((unsigned int)entry[29] << 8) |
                ((unsigned int)entry[30] << 16) |
                ((unsigned int)entry[31] << 24);

            break;
        }

        if (file_cluster != 0)
            break;
    }

    if (file_cluster == 0) {
        print("FAT16: File not found\n");
        return;
    }

    /*
     * Count clusters.
     */

    unsigned int cluster_count = 0;
    unsigned short current_cluster = file_cluster;

    while (current_cluster >= 2 &&
           current_cluster < 0xFFF8) {

        cluster_count++;

        unsigned short next_cluster =
            fat16_get_next_cluster(current_cluster);

        if (next_cluster == 0) {
            print("FAT16: Invalid cluster chain\n");
            return;
        }

        current_cluster = next_cluster;
    }

    print("Name: ");
    print(filename);
    print("\n");

    print("Size: ");

    char size_buffer[12];
    unsigned int value = file_size;
    int position = 0;

    if (value == 0) {
        size_buffer[position++] = '0';
    } else {
        char reverse[12];
        int reverse_position = 0;

        while (value > 0) {
            reverse[reverse_position++] =
                '0' + (value % 10);

            value /= 10;
        }

        while (reverse_position > 0)
            size_buffer[position++] =
                reverse[--reverse_position];
    }

    size_buffer[position] = '\0';

    print(size_buffer);
    print(" bytes\n");

    print("First cluster: ");

    value = file_cluster;
    position = 0;

    if (value == 0) {
        size_buffer[position++] = '0';
    } else {
        char reverse[12];
        int reverse_position = 0;

        while (value > 0) {
            reverse[reverse_position++] =
                '0' + (value % 10);

            value /= 10;
        }

        while (reverse_position > 0)
            size_buffer[position++] =
                reverse[--reverse_position];
    }

    size_buffer[position] = '\0';

    print(size_buffer);
    print("\n");

    print("Clusters: ");

    value = cluster_count;
    position = 0;

    if (value == 0) {
        size_buffer[position++] = '0';
    } else {
        char reverse[12];
        int reverse_position = 0;

        while (value > 0) {
            reverse[reverse_position++] =
                '0' + (value % 10);

            value /= 10;
        }

        while (reverse_position > 0)
            size_buffer[position++] =
                reverse[--reverse_position];
    }

    size_buffer[position] = '\0';

    print(size_buffer);
    print("\n");
}

// command: del <filename>
void ata_delete(char* filename)
{
    if (fat16.bytes_per_sector == 0) {
        print("FAT16: Not mounted\n");
        return;
    }

    if (filename == 0 || filename[0] == '\0') {
        print("Usage: del <filename>\n");
        return;
    }

    unsigned int root_dir_start =
        fat16.reserved_sectors +
        ((unsigned int)fat16.fat_count * fat16.sectors_per_fat);

    unsigned int root_dir_sectors =
        ((unsigned int)fat16.root_entries * 32 +
         fat16.bytes_per_sector - 1) /
        fat16.bytes_per_sector;

    unsigned char buffer[512];

    char name[8];
    char ext[3];

    for (int i = 0; i < 8; i++)
        name[i] = ' ';

    for (int i = 0; i < 3; i++)
        ext[i] = ' ';

    int i = 0;
    int j = 0;

    while (filename[i] != '\0' &&
           filename[i] != '.' &&
           i < 8) {

        char c = filename[i];

        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';

        name[i] = c;
        i++;
    }

    if (filename[i] == '.') {
        i++;

        while (filename[i] != '\0' && j < 3) {
            char c = filename[i];

            if (c >= 'a' && c <= 'z')
                c -= 'a' - 'A';

            ext[j++] = c;
            i++;
        }
    }

    for (unsigned int sector = 0;
         sector < root_dir_sectors;
         sector++) {

        if (!ata_read_sector(
                ATA_DRIVE_SLAVE,
                root_dir_start + sector,
                buffer)) {

            print("FAT16: Directory read error\n");
            return;
        }

        for (unsigned int offset = 0;
             offset < fat16.bytes_per_sector;
             offset += 32) {

            unsigned char* entry = &buffer[offset];

            if (entry[0] == 0x00)
                return;

            if (entry[0] == 0xE5)
                continue;

            if (entry[11] == 0x0F)
                continue;

            if (entry[11] & 0x08)
                continue;

            int match = 1;

            for (int k = 0; k < 8; k++) {
                if (entry[k] != (unsigned char)name[k]) {
                    match = 0;
                    break;
                }
            }

            if (match) {
                for (int k = 0; k < 3; k++) {
                    if (entry[8 + k] !=
                        (unsigned char)ext[k]) {

                        match = 0;
                        break;
                    }
                }
            }

            if (!match)
                continue;

            unsigned short cluster =
                entry[26] |
                ((unsigned short)entry[27] << 8);

            print("FAT16: File found\n");

            if (cluster >= 2) {
                unsigned short current_cluster = cluster;

                while (current_cluster >= 2 &&
                       current_cluster < 0xFFF8) {

                    unsigned short next_cluster =
                        fat16_get_next_cluster(current_cluster);

                    if (!fat16_set_cluster(
                            current_cluster,
                            FAT16_CLUSTER_FREE)) {

                        print("FAT16: Failed to free cluster\n");
                        return;
                    }

                    current_cluster = next_cluster;
                }

                print("FAT16: Cluster chain freed\n");
            }

            entry[0] = 0xE5;

            if (!ata_write_sector(
                    ATA_DRIVE_SLAVE,
                    root_dir_start + sector,
                    buffer)) {

                print("FAT16: Directory write error\n");
                return;
            }

            print("FAT16: File deleted\n");
            return;
        }
    }

    print("FAT16: File not found\n");
}


// command: write <filename>
/* =========================================================================
 * Writing a buffer of set size in a FAT16 File
 * ========================================================================= */
int fat16_write_buffer(const char* filename, const char* buffer, unsigned int text_length)
{
    if (fat16.bytes_per_sector == 0) {
        return 0; // not mounted
    }

    if (!filename || filename[0] == '\0') {
        return 0;
    }

    // If file or already exists -> delete old version and save new!
    ata_delete((char*)filename);

    unsigned int cluster_size = (unsigned int)fat16.bytes_per_sector * fat16.sectors_per_cluster;
    unsigned int cluster_count = (text_length + cluster_size - 1) / cluster_size;
    if (cluster_count == 0) cluster_count = 1;

    unsigned short cluster = fat16_allocate_cluster_chain(cluster_count);
    if (cluster == 0) {
        return 0;
    }

    unsigned int data_start =
        fat16.reserved_sectors +
        ((unsigned int)fat16.fat_count * fat16.sectors_per_fat) +
        (((unsigned int)fat16.root_entries * 32 + fat16.bytes_per_sector - 1) / fat16.bytes_per_sector);

    unsigned short current_cluster = cluster;
    unsigned int text_offset = 0;

    while (current_cluster >= 2 && current_cluster < 0xFFF8 && text_offset < text_length) {
        unsigned int cluster_sector =
            data_start + ((unsigned int)(current_cluster - 2) * fat16.sectors_per_cluster);

        for (unsigned int sector = 0; sector < fat16.sectors_per_cluster && text_offset < text_length; sector++) {
            unsigned char data[512];
            for (unsigned int i = 0; i < 512; i++) data[i] = 0;

            unsigned int remaining = text_length - text_offset;
            unsigned int bytes_to_write = (remaining < 512) ? remaining : 512;

            for (unsigned int i = 0; i < bytes_to_write; i++) {
                data[i] = buffer[text_offset + i];
            }

            if (!ata_write_sector(ATA_DRIVE_SLAVE, cluster_sector + sector, data)) {
                return 0;
            }

            text_offset += bytes_to_write;
        }

        if (text_offset >= text_length) break;
        current_cluster = fat16_get_next_cluster(current_cluster);
        if (current_cluster == 0) return 0;
    }

    if (!fat16_create_root_entry(filename, cluster, text_length)) {
        return 0;
    }

    return 1; // Saved successfully
}

// ATA: mkdir <dirname>
void mkdir(char* path) {
    if (fat16.bytes_per_sector == 0) {
        print("FAT16: Not mounted\n");
        return;
    }

    if (path == 0 || path[0] == '\0') {
        print("Usage: mkdir <dirname>\n");
        return;
    }

    print("FAT16: Allocating directory cluster...\n");

    // 1. Selecting first cluster for the content of new directory
    unsigned short cluster = fat16_allocate_cluster_chain(1);
    if (cluster == 0) {
        print("FAT16: Failed to allocate cluster for directory\n");
        return;
    }

    // 2. Counting a starting data sector for selected cluster
    unsigned int data_start =
        fat16.reserved_sectors +
        ((unsigned int)fat16.fat_count * fat16.sectors_per_fat) +
        (((unsigned int)fat16.root_entries * 32 +
          fat16.bytes_per_sector - 1) /
         fat16.bytes_per_sector);

    unsigned int dir_sector =
        data_start +
        ((unsigned int)(cluster - 2) * fat16.sectors_per_cluster);

    // 3. Forming a starting sector with "." & ".."
    unsigned char block[512];
    for (int i = 0; i < 512; i++) {
        block[i] = 0;
    }

    // Запис "." (поточна директорія) -> зміщення 0
    block[0] = '.';
    for (int i = 1; i < 11; i++) block[i] = ' ';
    block[11] = 0x10; // Directory
    block[26] = cluster & 0xFF;
    block[27] = (cluster >> 8) & 0xFF;

    // Запис ".." (батьківська директорія: 0x0000 для Root) -> зміщення 32 (0x20)
    block[32] = '.';
    block[33] = '.';
    for (int i = 2; i < 11; i++) block[32 + i] = ' ';
    block[32 + 11] = 0x10; // Directory
    block[32 + 26] = 0; // Кластер 0 означає Root Directory
    block[32 + 27] = 0;

    // Записуємо перший сектор нової папки
    if (!ata_write_sector(ATA_DRIVE_SLAVE, dir_sector, block)) {
        print("FAT16: Failed to initialize directory block\n");
        return;
    }

    // Якщо в кластері більше одного сектора, зануляємо решту
    unsigned char zero_block[512];
    for (int i = 0; i < 512; i++) zero_block[i] = 0;
    for (unsigned int s = 1; s < fat16.sectors_per_cluster; s++) {
        ata_write_sector(ATA_DRIVE_SLAVE, dir_sector + s, zero_block);
    }

    // 4. Створюємо запис нової папки в кореневому каталозі
    if (!fat16_create_root_dir_entry(path, cluster)) {
        print("FAT16: Failed to create root entry for directory\n");
        return;
    }

    print("Directory created: ");
    print(path);
    print("\n");
}

// ATA: pwd
void pwd(void) {
    if (fat16.bytes_per_sector == 0) {
        print("FAT16: Not mounted\n");
        return;
    }
    print(current_path);
    print("\n");
}

// ATA: cd <dirname>
void chdir(char* path) {
    // if directory exists ->
    // -> change path

    // else (directory doesn't exist) ->
    // print("ERROR: No such file or directory -> ");
    // print(path);
    // print("\n");

    if (fat16.bytes_per_sector == 0) {
        print("FAT16: Not mounted\n");
        return;
    }

    if (path == 0 || path[0] == '\0') {
        print("Usage: cd <dirname>\n");
        return;
    }

    // 1. Перехід у корінь: cd / або cd <backslash>
    if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0) {
        current_dir_cluster = 0;
        current_path[0] = '/';
        current_path[1] = '\0';
        return;
    }

    // Підготовка імені для порівняння у форматі 8.3
    char name[8];
    char ext[3];
    for (int i = 0; i < 8; i++) name[i] = ' ';
    for (int i = 0; i < 3; i++) ext[i] = ' ';

    int i = 0;
    while (path[i] != '\0' && path[i] != '.' && i < 8) {
        char c = path[i];
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        name[i] = c;
        i++;
    }

    // Обробка "." та ".."
    if (strcmp(path, ".") == 0) {
        name[0] = '.';
    } else if (strcmp(path, "..") == 0) {
        name[0] = '.';
        name[1] = '.';
    }

    unsigned char buffer[512];
    unsigned short target_cluster = 0xFFFF;
    int is_dir = 0;

    // 2. Якщо ми в Root Directory (current_dir_cluster == 0)
    if (current_dir_cluster == 0) {
        unsigned int root_dir_start =
            fat16.reserved_sectors + ((unsigned int)fat16.fat_count * fat16.sectors_per_fat);
        unsigned int root_dir_sectors =
            ((unsigned int)fat16.root_entries * 32 + fat16.bytes_per_sector - 1) / fat16.bytes_per_sector;

        for (unsigned int sector = 0; sector < root_dir_sectors; sector++) {
            if (!ata_read_sector(ATA_DRIVE_SLAVE, root_dir_start + sector, buffer)) return;

            for (unsigned int offset = 0; offset < fat16.bytes_per_sector; offset += 32) {
                unsigned char* entry = &buffer[offset];
                if (entry[0] == 0x00) break;
                if (entry[0] == 0xE5 || entry[11] == 0x0F || (entry[11] & 0x08)) continue;

                int match = 1;
                for (int k = 0; k < 8; k++) {
                    if (entry[k] != (unsigned char)name[k]) { match = 0; break; }
                }

                if (match && (entry[11] & 0x10)) { // Перевірка чи це папка
                    target_cluster = entry[26] | ((unsigned short)entry[27] << 8);
                    is_dir = 1;
                    break;
                }
            }
            if (is_dir) break;
        }
    }
    // 3. Якщо ми вже всередині підпапки (current_dir_cluster >= 2)
    else {
        unsigned int data_start =
            fat16.reserved_sectors +
            ((unsigned int)fat16.fat_count * fat16.sectors_per_fat) +
            (((unsigned int)fat16.root_entries * 32 + fat16.bytes_per_sector - 1) / fat16.bytes_per_sector);

        unsigned short cluster = current_dir_cluster;

        while (cluster >= 2 && cluster < 0xFFF8) {
            unsigned int cluster_sector =
                data_start + ((unsigned int)(cluster - 2) * fat16.sectors_per_cluster);

            for (unsigned int s = 0; s < fat16.sectors_per_cluster; s++) {
                if (!ata_read_sector(ATA_DRIVE_SLAVE, cluster_sector + s, buffer)) return;

                for (unsigned int offset = 0; offset < fat16.bytes_per_sector; offset += 32) {
                    unsigned char* entry = &buffer[offset];
                    if (entry[0] == 0x00) break;
                    if (entry[0] == 0xE5 || entry[11] == 0x0F) continue;

                    int match = 1;
                    for (int k = 0; k < 8; k++) {
                        if (entry[k] != (unsigned char)name[k]) { match = 0; break; }
                    }

                    if (match && (entry[11] & 0x10)) {
                        target_cluster = entry[26] | ((unsigned short)entry[27] << 8);
                        is_dir = 1;
                        break;
                    }
                }
                if (is_dir) break;
            }
            if (is_dir) break;
            cluster = fat16_get_next_cluster(cluster);
        }
    }

    // 4. Результат пошуку
    if (!is_dir || target_cluster == 0xFFFF) {
        print("cd: No such directory: ");
        print(path);
        print("\n");
        return;
    }

    current_dir_cluster = target_cluster;

    // Оновлюємо відображення шляху (current_path)
    if (strcmp(path, "..") == 0) {
        if (current_dir_cluster == 0) {
            current_path[0] = '/';
            current_path[1] = '\0';
        } else {
            // Відкатуємо шлях до попереднього скісного
            int len_p = 0;
            while (current_path[len_p]) len_p++;
            while (len_p > 1 && current_path[len_p - 1] != '/') {
                len_p--;
            }
            if (len_p > 1) len_p--; // прибираємо зайвий скісний
            current_path[len_p] = '\0';
        }
    } else if (strcmp(path, ".") != 0) {
        int len_p = 0;
        while (current_path[len_p]) len_p++;
        if (len_p > 1) current_path[len_p++] = '/';
        int k = 0;
        while (path[k]) {
            current_path[len_p++] = path[k++];
        }
        current_path[len_p] = '\0';
    }
}

