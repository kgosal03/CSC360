#include "diskutils.h"

// For reading the boot sector
void read_boot_sector(FILE *disk, struct fat_12_boot_sector *boot_sector) {
    if (fseek(disk, 0, SEEK_SET) != 0) {
        perror("Failed seeking disk beginning");
        exit(EXIT_FAILURE); 
    };
    if (fread(boot_sector, sizeof(struct fat_12_boot_sector), 1, disk) != 1) {
        perror("Failed reading boot sector");
        exit(EXIT_FAILURE);
    }
}

// For reading the FAT
void read_fat(FILE *disk, struct fat_12_boot_sector *boot_sector, uint8_t *FAT) {
    // Calculate the size of the FAT in bytes
    size_t fat_size = boot_sector->sector_per_fat * boot_sector->bytes_per_sec;

    // Seek to the start of the FAT
    if (fseek(disk, 512, SEEK_SET) != 0) {
        perror("Failed to seek to FAT");
        exit(EXIT_FAILURE);
    }

    // Read the FAT into the buffer
    if (fread(FAT, fat_size, 1, disk) != 1) {
        perror("Failed to read FAT");
        exit(EXIT_FAILURE);
    }
}

// For getting the fat entry
int get_FAT_entry(uint8_t *FAT, int n) {
    int offset = (n * 3) / 2;
    int fat_entry;

    // Entry is even
    if (n % 2 == 0) {
        fat_entry = FAT[offset] | ((FAT[offset + 1] & 0x0F) << 8);
    }
    // Entry is odd
    else {
        fat_entry = ((FAT[offset] & 0xF0) >> 4) | (FAT[offset + 1] << 4);
    }
    return fat_entry;
}