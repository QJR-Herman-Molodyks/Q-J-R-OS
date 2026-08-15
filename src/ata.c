
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

        if (!ata_read_sector(
                ATA_DRIVE_SLAVE,
                root_dir_start + sector,
                buffer)) {

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
void ata_ls(void) { // maybe path in args, don't know
    // print current directory / path files list
    if (fat16.bytes_per_sector == 0) {
        print("FAT16: Not mounted\n");
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

    print("Files:\n");

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

            // End of directory
            if (entry[0] == 0x00)
                return;

            // Deleted entry
            if (entry[0] == 0xE5)
                continue;

            // Long File Name entry
            if (entry[11] == 0x0F)
                continue;

            // Volume label
            if (entry[11] & 0x08)
                continue;

            // Filename: 8 characters
            for (int i = 0; i < 8; i++) {
                if (entry[i] == ' ')
                    break;

                put_char(entry[i]);
            }

            // Extension
            if (entry[8] != ' ') {
                put_char('.');

                for (int i = 8; i < 11; i++) {
                    if (entry[i] == ' ')
                        break;

                    put_char(entry[i]);
                }
            }

            print("\n");
             }
         }
}

// command: write <filename>
void ata_write(char* filename) {
    // here will be a simple text editor:
    // keys:
    // ESC - Exit
    // F1  - Save
    //
    // That's gonna look like that:
    // Q-J-R OS Writer v1.0: <filename>: ESC = Exit; F1 = Save.
    // ~ Hello
    // ~ World_

    // This is fully CLI text editor (with input lines)
    if (fat16.bytes_per_sector == 0) {
        print("FAT16: Not mounted\n");
        return;
    }

    if (filename == 0 || filename[0] == '\0') {
        print("Usage: write <filename>\n");
        return;
    }

    // print("FAT16: Searching free cluster...\n");

    // unsigned short cluster = fat16_find_free_cluster();

    // if (cluster == 0) {
    //    print("FAT16: No free clusters\n");
    //    return;
    // }

    // print("FAT16: Free cluster found\n");

    // if (!fat16_set_cluster(cluster,FAT16_CLUSTER_EOF)) {
    //    print("FAT16: Failed to allocate cluster\n");
    //    return;
    // }

    // print("FAT16: Cluster allocated\n");

	print("FAT16: Preparing data...\n");

	const char* text =
    	"Q-J-R OS FAT16 MULTI-CLUSTER TEST\n"
    	"Cluster chain works correctly!\n"
    	"LETS GOOOOOOOOOOOO!!!!!\n";

	unsigned int text_length = 0;

	while (text[text_length] != '\0')
    	text_length++;

	unsigned int cluster_size =
    	(unsigned int)fat16.bytes_per_sector *
    	fat16.sectors_per_cluster;

	unsigned int cluster_count =
    	(text_length + cluster_size - 1) /
    	cluster_size;

	if (cluster_count == 0)
    	cluster_count = 1;

	print("FAT16: Allocating cluster chain...\n");

	unsigned short cluster =
    	fat16_allocate_cluster_chain(cluster_count);

	if (cluster == 0) {
    	print("FAT16: Failed to allocate cluster chain\n");
    	return;
	}

	print("FAT16: Cluster chain allocated\n");

	// unsigned char data[2048];

	// for (int i = 0; i < 2048; i++)
    //	data[i] = 0;

	// const char* text = "Hello from Q-J-R OS!";

	// int text_length = 0;

	// while (text[text_length] != '\0')
    //	text_length++;

	// for (int i = 0; i < text_length; i++)
    //	data[i] = text[i];

unsigned int data_start =
    fat16.reserved_sectors +
    ((unsigned int)fat16.fat_count *
     fat16.sectors_per_fat) +
    (((unsigned int)fat16.root_entries * 32 +
      fat16.bytes_per_sector - 1) /
     fat16.bytes_per_sector);

unsigned short current_cluster = cluster;

unsigned int text_offset = 0;

while (current_cluster >= 2 &&
       current_cluster < 0xFFF8 &&
       text_offset < text_length) {

    unsigned int cluster_sector =
        data_start +
        ((unsigned int)(current_cluster - 2) *
         fat16.sectors_per_cluster);

    for (unsigned int sector = 0;
         sector < fat16.sectors_per_cluster &&
         text_offset < text_length;
         sector++) {

        unsigned char data[512];

        for (unsigned int i = 0; i < 512; i++)
            data[i] = 0;

        unsigned int remaining =
            text_length - text_offset;

        unsigned int bytes_to_write = 512;

        if (remaining < bytes_to_write)
            bytes_to_write = remaining;

        for (unsigned int i = 0;
             i < bytes_to_write;
             i++) {

            data[i] = text[text_offset + i];
        }

        if (!ata_write_sector(
                ATA_DRIVE_SLAVE,
                cluster_sector + sector,
                data)) {

            print("FAT16: Data write error\n");
            return;
        }

        text_offset += bytes_to_write;
    }

    if (text_offset >= text_length)
        break;

    current_cluster =
        fat16_get_next_cluster(current_cluster);

    if (current_cluster == 0) {
        print("FAT16: Invalid cluster chain\n");
        return;
    }
}

print("FAT16: Data written\n");

if (!fat16_create_root_entry(
        filename,
        cluster,
        (unsigned int)text_length)) {

    print("FAT16: Failed to create file\n");
    return;
}

print("FAT16: File created\n");
}

// command: read <filename>
//void ata_read(char* filename) {
    // reading filename:
    // Content of <filename>:
    // <content>
//}

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
