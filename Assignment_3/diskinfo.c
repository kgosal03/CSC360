#include "diskutils.h"

// To get the volume label in case not in the boot sector
void get_volume_label(FILE *disk, struct fat_12_boot_sector *boot_sector) {
    // Seek to the root directory
    long root_directory_start = 9728;

    if (fseek(disk, root_directory_start, SEEK_SET) != 0) {
        perror("Failed seeking the root directory");
        exit(EXIT_FAILURE);
    }

    // Read root directory entries
    for (int i = 0; i < boot_sector->max_num_root_dirs; i++) {
        struct directory_entry entry;

        if (fread(&entry, sizeof(struct directory_entry), 1, disk) != 1) {
            perror("Failed to read directory entry");
            exit(EXIT_FAILURE);
        }

        // Check for the volume label entry
        if (entry.attributes == 0x08) {
            if (boot_sector == NULL || entry.filename[0] == '\0' || entry.extension[0] == '\0') {
                fprintf(stderr, "Null pointer encountered\n");
                exit(EXIT_FAILURE);
            }

            if (sizeof(boot_sector->vol_label) < 11) {
                fprintf(stderr, "Destination buffer for vol_label is too small\n");
                exit(EXIT_FAILURE);
            }

            // Volume label found
            memcpy(boot_sector->vol_label, entry.filename, 8);
            memcpy(boot_sector->vol_label + 8, entry.extension, 3);
            // To null-terminate the label
            boot_sector->vol_label[10] = '\0';
            return;
        }
    }
}

// To print the inforamtion about the disk
void print_disk_info(FILE *disk, struct fat_12_boot_sector *boot_sector, int free_sectors, int file_count) {
    char blank_label[11];
    memset(blank_label, ' ', sizeof(blank_label));

    // If volume label not in the boot sector
    if (memcmp(boot_sector->vol_label, blank_label, 11) == 0) {
        get_volume_label(disk, boot_sector);
    }

    // Printing the required information
    printf("OS Name: %.8s\n", boot_sector->os_name);
    printf("Label of the disk: %.11s\n", boot_sector->vol_label);
    printf("Total size of the disk: %u bytes\n", boot_sector->total_sector_count * boot_sector->bytes_per_sec);
    printf("Free size of the disk: %d bytes\n", free_sectors * boot_sector->bytes_per_sec);
    printf("\n================\n");
    printf("The number of files in the disk: %d\n", file_count);
    printf("(including all files in the root directory and files in all subdirectories)\n");
    printf("================\n");
    printf("\nNumber of FAT copies: %u\n", boot_sector->num_fat);
    printf("Sectors per FAT: %u\n", boot_sector->sector_per_fat);
}

// Count the number of free sectors
int count_free_sectors(uint8_t *FAT, uint16_t size) {
    int num_free_sector = 0;
    int fat_entry = 3;

    // First two entries in fat are reserved
    for (int i = 2; i < size; i++) {
        if (fat_entry % 2 == 1) {
            if (FAT[i * 3 / 2] == 0x00 && (FAT[i * 3 / 2 + 1] & 0x0F) == 0x00) {
                num_free_sector++;
            }
        }
        if (fat_entry % 2 == 0) {
            if ((FAT[i * 3 / 2] & 0xF0) == 0x00 && FAT[i * 3 / 2 + 1] == 0x00) {
                num_free_sector++;
            }
        }
        fat_entry++;
    }
    return num_free_sector;
}

// Recursive function to check if any files in the subdirectories, if any update count accordingly
void read_subdirectory(FILE *disk, unsigned char *FAT, struct directory_entry *dir_entry, struct fat_12_boot_sector *boot_sector, int *file_count) {
    // Compute the cluster size and the number of entries per sector
    uint16_t sectors_per_cluster = boot_sector->sectors_per_cluster;
    uint16_t bytes_per_sector = boot_sector->bytes_per_sec;
    uint16_t entries_per_sector = bytes_per_sector / sizeof(struct directory_entry);

    // Calculate the starting sector of the directory
    uint16_t start_cluster = dir_entry->first_logical_cluster;
    uint16_t current_cluster = start_cluster;

    // Save the current position of the file pointer
    long original_position = ftell(disk);

    // Check if ftell is successful
    if (original_position == -1L) {
        perror("Failed to get the current file position");
        exit(EXIT_FAILURE);
    }

    // Buffer to read directory entries
    struct directory_entry entries[entries_per_sector];

    // Loops in case more than one cluster is there
    while (current_cluster >= 2 && current_cluster < 0xFF8) {
        // Calculate the starting sector of the current cluster
        uint16_t start_sector = 33 + (current_cluster - 2) * sectors_per_cluster;

        // Seek to the starting sector of the current cluster
        long offset = start_sector * bytes_per_sector;
        if (fseek(disk, offset, SEEK_SET) != 0) {
            perror("Failed to seek to the starting sector of the current cluster");
            exit(EXIT_FAILURE);
        }

        size_t read_count = fread(entries, sizeof(struct directory_entry), entries_per_sector, disk);
        // Check if fread succeeded in reading the expected number of entries
        if (read_count != entries_per_sector) {
            if (ferror(disk)) {
                perror("Error reading from disk");
                exit(EXIT_FAILURE);
            }
        }

        // Checking every entry in the sector
        for (int i = 0; i < entries_per_sector; i++) {
            struct directory_entry entry = entries[i];

            // If free directory entry or '.' and '..' entries
            if (entry.filename[0] == 0x00 || entry.filename[0] == 0xE5 || (entry.filename[0] == '.' && (entry.filename[1] == ' ' || entry.filename[1] == '.'))) {
                continue;
            }

            // If the entry is a file
            if ((entry.attributes & 0x10) == 0 && entry.first_logical_cluster != 0 && entry.first_logical_cluster != 1 && (entry.attributes & 0x02) == 0) {
                (*file_count)++;
            }

            // If a directory, we recursively read subdirectories
            if ((entry.attributes & 0x10) != 0) {
                read_subdirectory(disk, FAT, &entry, boot_sector, file_count);
            }
        }

        // Getting the next cluster from the FAT Entry table
        current_cluster = get_FAT_entry(FAT, current_cluster);
    }

    // Restore the original position of the file pointer
    if (fseek(disk, original_position, SEEK_SET) != 0) {
        perror("Failed to seek to the original position of the file pointer");
        exit(EXIT_FAILURE);
    }
}

// To count the number of files in the disk
void count_files(FILE *disk, uint8_t *FAT, struct fat_12_boot_sector *boot_sector, int *file_count) {
    // Seeking to the root directory
    if (fseek(disk, 9728, SEEK_SET) != 0) {
        perror("Failed to seek to position 9728 that is root directory");
        exit(EXIT_FAILURE);
    }

    // Traversing through all the root dir entries
    for (int i = 0; i < boot_sector->max_num_root_dirs; i++) {
        struct directory_entry entry;

        if (fread(&entry, sizeof(struct directory_entry), 1, disk) != 1) {
            perror("Failed to read directory entry");
            exit(EXIT_FAILURE);
        }

        // If free dir with no file or dir in it
        if (entry.filename[0] == 0x00 || entry.filename[0] == 0xE5) {
            continue;
        }

        // If the entry is a file
        if ((entry.attributes & 0x10) == 0 && entry.first_logical_cluster != 0 && entry.first_logical_cluster != 1 && (entry.attributes & 0x02) == 0) {
            (*file_count)++;
        }
        
        // If a directory, we recursively read subdirectories
        if ((entry.attributes & 0x10) == 0x10) {
            read_subdirectory(disk, FAT, &entry, boot_sector, file_count);
        }
    }
}

int main(int argc, char *argv[]) {
    // Check number of arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk image file>\n", argv[0]);
        return 1;
    }

    // Open the disk
    FILE *disk = fopen(argv[1], "rb");
    if (!disk) {
        perror("Failed: Cannot open the disk image file");
        return 1;
    }

    // Read the boot sector
    struct fat_12_boot_sector boot_sector;
    read_boot_sector(disk, &boot_sector);

    // Allocationg space for the FAT and reading FAT
    uint8_t *FAT = malloc(boot_sector.sector_per_fat * boot_sector.bytes_per_sec);
    // Check if malloc succeeded
    if (FAT == NULL) {
        perror("Failed to allocate memory for FAT");
        // Close the disk
        if (fclose(disk) != 0) {
            perror("Failed to close the file");
            exit(EXIT_FAILURE);
        }
        return 1;
    }
    read_fat(disk, &boot_sector, FAT);

    // Counting free sector for free space and files
    int free_sectors = count_free_sectors(FAT, (boot_sector.total_sector_count-33+2));
    int file_count = 0;
    count_files(disk, FAT, &boot_sector, &file_count);

    // Print the disk information
    print_disk_info(disk, &boot_sector, free_sectors, file_count);

    free(FAT);

    // Close the disk
    if (fclose(disk) != 0) {
        perror("Failed to close the file");
        exit(EXIT_FAILURE);
    }
    return 0;
}
