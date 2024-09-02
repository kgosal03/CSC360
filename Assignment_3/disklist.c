#include "diskutils.h"

// Print the dir entry as per the format desired
void print_entry_info(struct directory_entry *entry, char type) {
    // Getting the date and time
    uint16_t time = entry->creation_time;
    uint16_t date = entry->creation_date;
    int hours = (time >> 11) & 0x1F;
    int minutes = (time >> 5) & 0x3F;
    int year = ((date >> 9) & 0x7F) + 1980;
    int month = (date >> 5) & 0x0F;
    int day = date & 0x1F;

    // For filename 8 chars and null terminator
    char filename[9];
    char full_filename[21];
    memcpy(filename, entry->filename, 8);
    filename[8] = '\0';

    // If a type 'F' as file, otherwise dir
    if (type == 'F') {
        // 4 chars for extension and null terminator
        char extension[4];
        memcpy(extension, entry->extension, 3);
        extension[3] = '\0';

        // Remove trailing spaces from filename and extension
        for (int i = 7; i >= 0 && filename[i] == ' '; --i) filename[i] = '\0';
        for (int i = 2; i >= 0 && extension[i] == ' '; --i) extension[i] = '\0';

        // Combining filename and extension into a single string
        snprintf(full_filename, sizeof(full_filename), "%s.%s", filename, extension);
    }
    else {
        for (int i = 7; i >= 0 && filename[i] == ' '; --i) filename[i] = '\0';
        snprintf(full_filename, sizeof(full_filename), "%s", filename);
    }
    
    // Printing the information
    fprintf(stdout, "%c %-10u %-20s %04d-%02d-%02d %02d:%02d\n\n",
        type, entry->file_size, full_filename,
        year, month, day, hours, minutes
    );
}

// Recursive function to check if any files or dirs in the subdirectories, if any print accordingly
void read_subdirectory(FILE *disk, unsigned char *FAT, struct directory_entry *dir_entry, struct fat_12_boot_sector *boot_sector) {
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

        // Checking every entry in the sector for printing
        for (int i = 0; i < entries_per_sector; i++) {
            struct directory_entry entry = entries[i];

            // If free directory entry or '.' and '..' entries
            if (entry.filename[0] == 0x00 || entry.filename[0] == 0xE5 || (entry.filename[0] == '.' && (entry.filename[1] == ' ' || entry.filename[1] == '.'))) {
                continue;
            }

            // If the entry is a file
            if ((entry.attributes & 0x10) == 0 && entry.first_logical_cluster != 0 && entry.first_logical_cluster != 1 && (entry.attributes & 0x02) == 0) {
                print_entry_info(&entry, 'F');
            }

            // If a directory
            if ((entry.attributes & 0x10) != 0) {
                print_entry_info(&entry, 'D');
            }
        }

        // Now recursing
        for (int i = 0; i < entries_per_sector; i++) {
            struct directory_entry entry = entries[i];

            // If free directory entry or '.' and '..' entries
            if (entry.filename[0] == 0x00 || entry.filename[0] == 0xE5 || (entry.filename[0] == '.' && (entry.filename[1] == ' ' || entry.filename[1] == '.'))) {
                continue;
            }

            // If a directory, we recursively read subdirectories
            if ((entry.attributes & 0x10) != 0) {
                fprintf(stdout, "\n/%.20s\n", entry.filename);
                printf("===========================================\n");
                read_subdirectory(disk, FAT, &entry, boot_sector);
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

// To print the disk entries starting from the root
void list_files(FILE *disk, uint8_t *FAT, struct fat_12_boot_sector *boot_sector) {
    // Seeking the root directory
    if (fseek(disk, 9728, SEEK_SET) != 0) {
        perror("Failed to seek to position 9728 that is root directory");
        exit(EXIT_FAILURE);
    }

    printf("ROOT\n");
    printf("===========================================\n");

    // Going over all the root dir entries for printing
    for (int i = 0; i < boot_sector->max_num_root_dirs; i++) {
        struct directory_entry entry;
        if (fread(&entry, sizeof(struct directory_entry), 1, disk) != 1) {
            perror("Failed to read directory entry");
            exit(EXIT_FAILURE);
        }

        // If free dir and no file or dir in it
        if (entry.filename[0] == 0x00 || entry.filename[0] == 0xE5) {
            continue;
        }

        // Check if the entry is a file
        if ((entry.attributes & 0x10) == 0 && entry.first_logical_cluster != 0 && entry.first_logical_cluster != 1 && (entry.attributes & 0x02) == 0) {
            print_entry_info(&entry, 'F');
        }
        
        // If a directory
        if ((entry.attributes & 0x10) == 0x10) {
            print_entry_info(&entry, 'D');
        }
    }

    // Now recursing through the sub directories
    if (fseek(disk, 9728, SEEK_SET) != 0) {
        perror("Failed to seek to position 9728 that is root directory");
        exit(EXIT_FAILURE);
    }

    // Going over all the root dir entries for looking into subdirs
    for (int i = 0; i < boot_sector->max_num_root_dirs; i++) {
        struct directory_entry entry;
        if (fread(&entry, sizeof(struct directory_entry), 1, disk) != 1) {
            perror("Failed to read directory entry");
            exit(EXIT_FAILURE);
        }

        // If free dir and no file or dir in it
        if (entry.filename[0] == 0x00 || entry.filename[0] == 0xE5) {
            continue;
        }

        // If a directory, recursively read subdirectories
        if ((entry.attributes & 0x10) == 0x10) {
            fprintf(stdout, "\n/%.20s\n", entry.filename);
            printf("===========================================\n");
            read_subdirectory(disk, FAT, &entry, boot_sector);
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

    // Listing the disk information
    list_files(disk, FAT, &boot_sector);

    free(FAT);

    // Close the disk
    if (fclose(disk) != 0) {
        perror("Failed to close the file");
        exit(EXIT_FAILURE);
    }
    return 0;
}