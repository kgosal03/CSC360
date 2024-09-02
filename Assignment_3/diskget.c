
#include "diskutils.h"
#include <ctype.h>

// To get the entry filename
void get_entry_filename(struct directory_entry *entry, char *full_filename) {
    char filename[9], extension[4];
    
    memcpy(filename, entry->filename, 8);
    filename[8] = '\0';

    memcpy(extension, entry->extension, 3);
    extension[3] = '\0';

    // Remove trailing spaces from filename and extension
    for (int i = 7; i >= 0 && filename[i] == ' '; --i) filename[i] = '\0';
    for (int i = 2; i >= 0 && extension[i] == ' '; --i) extension[i] = '\0';

    // Combine filename and extension into a single string
    snprintf(full_filename, 21, "%s.%s", filename, extension);

    // Convert full_filename to uppercase
    for (char *p = full_filename; *p; ++p) {
        *p = toupper((unsigned char)*p);
    }
}

// Copying the file from the disk if found
void get_file_from_disk(FILE *disk, uint8_t *FAT, struct directory_entry *dir_entry, struct fat_12_boot_sector *boot_sector, const char *output_filename) {
    uint16_t bytes_per_sector = boot_sector->bytes_per_sec;
    uint16_t start_cluster = dir_entry->first_logical_cluster;
    uint16_t current_cluster = start_cluster;
    uint32_t file_size = dir_entry->file_size;

    // File for writing out
    FILE *output_file = fopen(output_filename, "wb");
    if (!output_file) {
        perror("Failed to create output file");
        exit(EXIT_FAILURE);
    }

    // If more than one cluster is used by the file
    while (current_cluster >= 2 && current_cluster < 0xFF8 && file_size > 0) {
        // Calculate the starting sector of the current cluster
        uint16_t start_sector = 33 + (current_cluster - 2);
        long offset = start_sector * bytes_per_sector;

        if (fseek(disk, offset, SEEK_SET) != 0) {
            perror("Failed to seek to the starting sector of the current cluster");
            exit(EXIT_FAILURE);
        }

        uint8_t buffer[bytes_per_sector];
        size_t bytes_to_read = (file_size < bytes_per_sector) ? file_size : bytes_per_sector;

        // Reading the file
        size_t read_bytes = fread(buffer, 1, bytes_to_read, disk);
        if (read_bytes != bytes_to_read) {
            if (ferror(disk)) {
                perror("Error reading from disk");
                exit(EXIT_FAILURE);
            }
            // Close the output file
            if (fclose(output_file) != 0) {
                perror("Failed to close the output file");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_FAILURE);
        }

        // Writing the file out
        if (fwrite(buffer, 1, bytes_to_read, output_file) != bytes_to_read) {
            perror("Error writing data to output file");
            exit(EXIT_FAILURE);
        }

        // Remaining file size
        file_size -= bytes_to_read;
        
        // Get the FAT entry val
        current_cluster = get_FAT_entry(FAT, current_cluster);
    }

    if (fclose(output_file) != 0) {
        perror("Failed to close the output file");
        exit(EXIT_FAILURE);
    }
}

// Searching if the file exists on the disk
void search_file_from_disk(FILE *disk, uint8_t *FAT, struct fat_12_boot_sector *boot_sector, char *filename) {
    // Seeking the root dir to start from
    if (fseek(disk, 9728, SEEK_SET) != 0) {
        perror("Failed to seek to position 9728 that is root directory");
        exit(EXIT_FAILURE);
    }

    // Iterating through all root entries
    for (int i = 0; i < boot_sector->max_num_root_dirs; i++) {
        struct directory_entry entry;
        if (fread(&entry, sizeof(struct directory_entry), 1, disk) != 1) {
            perror("Failed to read directory entry");
            exit(EXIT_FAILURE);
        }

        // If free entry
        if (entry.filename[0] == 0x00 || entry.filename[0] == 0xE5 || (entry.attributes & 0x10) == 0x10) {
            continue;
        }

        // If entry is a file
        if ((entry.attributes & 0x10) == 0 && entry.first_logical_cluster != 0 && entry.first_logical_cluster != 1 && (entry.attributes & 0x02) == 0) {
            // Get the entry file name
            char full_filename[21];
            get_entry_filename(&entry, full_filename);

            // Making the name upper case
            for (char *p = filename; *p; ++p) {
                *p = toupper((unsigned char)*p);
            }

            // File found
            if (strcmp(full_filename, filename) == 0) {
                get_file_from_disk(disk, FAT, &entry, boot_sector, filename);
                return;
            }
        }
    }

    fprintf(stderr, "File not found.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // Check number of arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image file> <filename>\n", argv[0]);
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

    // Look for the file on the disk and then get if found
    search_file_from_disk(disk, FAT, &boot_sector, argv[2]);

    free(FAT);
    // Close the disk
    if (fclose(disk) != 0) {
        perror("Failed to close the file");
        exit(EXIT_FAILURE);
    }
    printf("Success!!\n");
    return 0;
}