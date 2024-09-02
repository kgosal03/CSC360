#include "diskutils.h"
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define MAX_PATH_LEN 256

void read_subdirectory(FILE *disk, uint8_t *FAT, struct directory_entry *dir_entry, struct fat_12_boot_sector *boot_sector, char *current_path, const char *search_path, uint16_t *found_cluster);

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

// Function to get file size, creation time, and last modification time
void get_file_info(const char *file_path, off_t *size, time_t *creation_time, time_t *modification_time) {
    struct stat file_stat;

    // Initialize output parameters
    if (size) *size = 0;
    if (creation_time) *creation_time = 0;
    if (modification_time) *modification_time = 0;

    // Get file status
    if (stat(file_path, &file_stat) != 0) {
        perror("stat");
        return;
    }

    // Use last modification time for both creation and modification times
    if (size) *size = file_stat.st_size;
    if (creation_time) *creation_time = file_stat.st_mtime;
    if (modification_time) *modification_time = file_stat.st_mtime;
}

void get_first_logical_cluster_dir(FILE *disk, uint8_t *FAT, struct fat_12_boot_sector *boot_sector, char *dir_path, uint16_t *found_cluster) {
    // Seek to the start of the root directory
    if (fseek(disk, 9728, SEEK_SET) != 0) {
        perror("Failed to seek to position 9728 that is root directory");
        exit(EXIT_FAILURE);
    }
    char current_path[MAX_PATH_LEN] = "/";

    // Iterate over all the root entries
    for (int i = 0; i < boot_sector->max_num_root_dirs; i++) {
        struct directory_entry entry;
        if (fread(&entry, sizeof(struct directory_entry), 1, disk) != 1) {
            perror("Failed to read directory entry");
            exit(EXIT_FAILURE);
        }

        // If free dir entry
        if (entry.filename[0] == 0x00 || entry.filename[0] == 0xE5) {
            continue;
        }

        char entry_name[9];
        snprintf(entry_name, sizeof(entry_name), "%.8s", entry.filename);
        // Removing trailing spaces
        for (int j = strlen(entry_name) - 1; j >= 0 && entry_name[j] == ' '; j--) {
            entry_name[j] = '\0';
        }

        // Required length for new_path
        size_t current_path_len = strlen(current_path);
        size_t entry_name_len = strlen(entry_name);
        size_t required_length = current_path_len + entry_name_len + 2;

        // Dynamically allocating memory for new_path
        char *new_path = (char *)malloc(required_length);
        if (new_path == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            return;
        }

        snprintf(new_path, required_length, "%s%s/", current_path, entry_name);

        // If a directory, process it
        if ((entry.attributes & 0x10) == 0x10) {
            // If this the dir we were looking for
            if (strcmp(new_path, dir_path) == 0) {
                *found_cluster = entry.first_logical_cluster;
                free(new_path);
                break;
            }
            // Recursively read subdirectories
            read_subdirectory(disk, FAT, &entry, boot_sector, new_path, dir_path, found_cluster);
        }

        free(new_path);

        if (*found_cluster != 0) {
            break;
        }
    }
}

// Checks if the desired directory is some subdirectory and get the first logical cluster
void read_subdirectory(FILE *disk, uint8_t *FAT, struct directory_entry *dir_entry, struct fat_12_boot_sector *boot_sector, char *current_path, const char *search_path, uint16_t *found_cluster) {
    uint16_t bytes_per_sec = boot_sector->bytes_per_sec;
    uint16_t entries_per_sector = bytes_per_sec / sizeof(struct directory_entry);
    uint16_t start_cluster = dir_entry->first_logical_cluster;
    uint16_t current_cluster = start_cluster;

    // Save the current position of the file pointer
    long original_position = ftell(disk);

    // Check if ftell is successful
    if (original_position == -1L) {
        perror("Failed to get the current file position");
        exit(EXIT_FAILURE);
    }

    // Buffer to read one sector of directory entries
    struct directory_entry entries[entries_per_sector];

    *found_cluster = 0; 

    while (current_cluster >= 2 && current_cluster < 0xFF8) {
        // Calculate the starting sector of the current cluster
        uint16_t start_sector = 33 + (current_cluster - 2);
        
        // Seek to the starting sector of the current cluster
        long offset = start_sector * bytes_per_sec;
        if (fseek(disk, offset, SEEK_SET) != 0) {
            perror("Failed to seek to the starting sector of the current cluster");
            exit(EXIT_FAILURE);
        }

        size_t read_count = fread(entries, sizeof(struct directory_entry), entries_per_sector, disk);
        // Check if fread successful
        if (read_count != entries_per_sector) {
            if (ferror(disk)) {
                perror("Error reading from disk");
                exit(EXIT_FAILURE);
            }
        }

        // Check each entry in the sector
        for (int i = 0; i < entries_per_sector; i++) {
            struct directory_entry entry = entries[i];

            // If a free directory entry
            if (entry.filename[0] == 0x00 || entry.filename[0] == 0xE5 || (entry.filename[0] == '.' && (entry.filename[1] == ' ' || entry.filename[1] == '.'))) {
                continue;
            }

            char entry_name[9];
            snprintf(entry_name, sizeof(entry_name), "%.8s", entry.filename);
            // Removing trailing spaces
            for (int j = strlen(entry_name) - 1; j >= 0 && entry_name[j] == ' '; j--) {
                entry_name[j] = '\0';
            }

            // Make sure not to exceed the max path length
            if (strlen(current_path) + strlen(entry_name) + 1 < MAX_PATH_LEN) {
                char new_path[MAX_PATH_LEN];
                snprintf(new_path, sizeof(new_path), "%s%s/", current_path, entry_name);

                // Check if the new path matches the search path
                if (strcmp(new_path, search_path) == 0) {
                    *found_cluster = entry.first_logical_cluster;
                    break;
                }

                // If a directory, recursively read subdirectories
                if ((entry.attributes & 0x10) != 0) {
                    read_subdirectory(disk, FAT, &entry, boot_sector, new_path, search_path, found_cluster);
                    if (*found_cluster != 0) {
                        break;
                    }
                }
            }
        }

        if (*found_cluster != 0) {
            break;
        }

        // Get the next cluster from the FAT
        current_cluster = get_FAT_entry(FAT, current_cluster);
    }

    // Restore the original position of the file pointer
    if (fseek(disk, original_position, SEEK_SET) != 0) {
        perror("Failed to seek to the original position of the file pointer");
        exit(EXIT_FAILURE);
    }
}

// Function to check if a file exists in linux file system
int file_exists(const char *file_path) {
    struct stat buffer;
    // Get the file status
    if (stat(file_path, &buffer) != 0) {
        perror("Error: File does not exist");
        // 0 if the file does not exist
        return 0; 
    }
     // 1 if file exists
    return 1;
}

// Function to extract directory path and file name from a given path
void extract_path_and_filename(const char *full_path, char *dir_path, char *file_name) {
    char *last_slash = strrchr(full_path, '/');
    if (last_slash) {
        strncpy(file_name, last_slash + 1, MAX_PATH_LEN - 1);
        file_name[MAX_PATH_LEN - 1] = '\0';  // Ensure null-termination
        strncpy(dir_path, full_path, last_slash - full_path);
        dir_path[last_slash - full_path] = '\0';
    } else {
        strcpy(file_name, full_path);
        dir_path[0] = '\0'; // Empty directory path
    }

    // Convert directory path to uppercase
    for (char *p = dir_path; *p; ++p) {
        *p = toupper((unsigned char)*p);
    }

    // Add a trailing slash to dir_path if not empty
    size_t len = strlen(dir_path);
    if (len > 0 && dir_path[len - 1] != '/') {
        dir_path[len] = '/';
        dir_path[len + 1] = '\0';
    }
}

// To get the next free FAT table entry
int find_free_fat_entry(uint8_t *FAT, uint16_t size) {
    int fat_entry = 3;
    for (int i = 2; i < size; i++) {
        // Odd entry
        if (fat_entry % 2 == 1) {
            if (FAT[i * 3 / 2] == 0x00 && (FAT[i * 3 / 2 + 1] & 0x0F) == 0x00) {
                return i;
            }
        }
        // Even entry
        if (fat_entry % 2 == 0) {
            if ((FAT[i * 3 / 2] & 0xF0) == 0x00 && FAT[i * 3 / 2 + 1] == 0x00) {
                return i;
            }
        }
        fat_entry++;
    }
    // If no free entry
    return -1;
}

// Function to mark entries in the FAT table
void mark_entries_in_fat(uint8_t *FAT, int *free_sectors, int sectors_needed, int fat_size) {
    // Iterates over the required needed required free sectors
    for (int i = 0; i < sectors_needed; i++) {
        free_sectors[i] = find_free_fat_entry(FAT, fat_size);

        if (free_sectors[i] == -1) {
            fprintf(stderr, "Error: No free FAT entry found.\n");
            exit(1);
        }

        // Previous entry to point at the current entry when more than one entries
        if (i > 0) {
            int previous = free_sectors[i - 1];
            int current = free_sectors[i];
            int previous_offset = (previous * 3) / 2;

            if (previous % 2 == 0) {
                FAT[previous_offset] = current & 0xFF;
                FAT[previous_offset + 1] = (FAT[previous_offset + 1] & 0xF0) | ((current >> 8) & 0x0F);
            }
            else {
                FAT[previous_offset] = (FAT[previous_offset] & 0x0F) | ((current << 4) & 0xF0);
                FAT[previous_offset + 1] = (current >> 4) & 0xFF;
            }
        }

        // Marking the current sector as used
        int current = free_sectors[i];
        int current_offset = (current * 3) / 2;

        if (current % 2 == 0) {
            FAT[current_offset] = 0xFF;
            FAT[current_offset + 1] = (FAT[current_offset + 1] & 0xF0) | 0x0F;
        } else {
            FAT[current_offset] = (FAT[current_offset] & 0x0F) | 0xF0;
            FAT[current_offset + 1] = 0xFF;
        }
    }

    // Last cluster in the file
    int last = free_sectors[sectors_needed - 1];
    int last_offset = (last * 3) / 2;

    if (last % 2 == 0) {
        FAT[last_offset] = 0xFF;
        FAT[last_offset + 1] = (FAT[last_offset + 1] & 0xF0) | 0x0F;
    }
    else {
        FAT[last_offset] = (FAT[last_offset] & 0x0F) | 0xF0;
        FAT[last_offset + 1] = 0xFF;
    }
}

// Function to convert time to FAT12 format
void convert_to_fat_time(struct tm *tm_info, uint16_t *date, uint16_t *time) {
    *date = ((tm_info->tm_year - 80) << 9) | ((tm_info->tm_mon + 1) << 5) | tm_info->tm_mday;
    *time = (tm_info->tm_hour << 11) | (tm_info->tm_min << 5) | (tm_info->tm_sec / 2);
}

// Function to seek and write FAT tables to the disk
void write_fat_to_disk(FILE *disk, long seek_position, const uint8_t *fat_data, size_t total_bytes) {
    // Seek to a position in the disk file
    if (fseek(disk, seek_position, SEEK_SET) != 0) {
        perror("Failed to seek to the specified position");
        exit(EXIT_FAILURE);
    }

    // Write FAT data to the disk
    if (fwrite(fat_data, 1, total_bytes, disk) != total_bytes) {
        perror("Error writing FAT to disk");
        exit(EXIT_FAILURE);
    }
}

// Write file to the root directory
void write_to_root_dir(FILE *disk, uint8_t *FAT, struct fat_12_boot_sector *boot_sector, char *filename) {
    int free_entry_found = 0;
    int free_entry_index = 0;

    // To keep the copy of the original filename
    char filename_copy[32];
    strncpy(filename_copy, filename, sizeof(filename_copy) - 1);
    filename_copy[sizeof(filename_copy) - 1] = '\0';
    
    // Seek to the start of the root directory
    if (fseek(disk, 9728, SEEK_SET) != 0) {
        perror("Failed to seek to position 9728 that is root directory");
        exit(EXIT_FAILURE);
    }

    // Iterating over all the entries in the root dir
    for (int i = 0; i < boot_sector->max_num_root_dirs; i++) {
        struct directory_entry entry;
        if (fread(&entry, sizeof(struct directory_entry), 1, disk) != 1) {
            perror("Failed to read directory entry");
            exit(EXIT_FAILURE);
        }

        // Flag that free entry is available and get the logical cluster number
        if (entry.filename[0] == 0xE5 || entry.filename[0] == 0x00) {
            if (!free_entry_found) {
                free_entry_found = 1;
                free_entry_index = i;
            }
            continue;
        }

        // Check if the filename matches
        char entry_name[13];
        snprintf(entry_name, 13, "%.8s.%.3s", entry.filename, entry.extension);
        entry_name[12] = '\0';
        // Remove padding
        for (int j = 0; j < 12; j++) {
            if (entry_name[j] == ' ') entry_name[j] = '\0';
        }

        // Capitalizing the filename for comparison
        for (char *p = filename; *p; ++p) {
            *p = toupper((unsigned char)*p);
        }

        // Throw error if same name entry found
        if (strcmp(entry_name, filename) == 0) {
            printf("Error: The file %s already exists in the root directory.\n", filename);
            exit(1);
        }
    }

    // If no free entry found in the root dir to store the new file
    if (!free_entry_found) {
        printf("Error: No free entry available in the root directory.\n");
        exit(1);
    }

    // Open the source file to be copied
    FILE *source_file = fopen(filename_copy, "rb");
    if (!source_file) {
        printf("Error: File not found or cannot be opened.\n");
        exit(1);
    }

    // Seek the end of source file to get file size
    if (fseek(source_file, 0, SEEK_END) != 0) {
        perror("Failed to seek to the end of the source file");
        exit(EXIT_FAILURE);
    }
    long file_size = ftell(source_file);

    // Check if ftell is successful
    if (file_size == -1L) {
        perror("Failed to get the file size");
        exit(EXIT_FAILURE);
    }

    // Seek the start of the source file to get the file data
    if (fseek(source_file, 0, SEEK_SET) != 0) {
        perror("Failed to seek to the start of the source file");
        exit(EXIT_FAILURE);
    }

    // Calculate the number of sectors needed
    int sectors_needed = (file_size + boot_sector->bytes_per_sec - 1) / boot_sector->bytes_per_sec;
    int free_sectors[sectors_needed];
    // Marks sectors as per the file requirements
    mark_entries_in_fat(FAT, free_sectors, sectors_needed, boot_sector->total_sector_count-33+2);
    
    // Write the file data to the disk image
    for (int i = 0; i < sectors_needed; i++) {
        uint8_t buffer[boot_sector->bytes_per_sec];
        size_t bytes_read = fread(buffer, 1, boot_sector->bytes_per_sec, source_file);

        if (bytes_read != boot_sector->bytes_per_sec) {
            if (ferror(source_file)) {
                perror("Error reading from source file");
                exit(EXIT_FAILURE);
            }
        }

        if (fseek(disk, ( 33 + free_sectors[i] - 2) * boot_sector->bytes_per_sec, SEEK_SET) != 0) {
            perror("Failed to seek to certain data sector");
            exit(EXIT_FAILURE);
        }

        if (fwrite(buffer, 1, boot_sector->bytes_per_sec, disk) != boot_sector->bytes_per_sec) {
            perror("Error writing to disk");
            exit(EXIT_FAILURE);
        }
    }

    // Update the directory entry in the root
    // Separate filename and extension with null terminate
    char just_filename[9] = {0};
    char extension[4] = {0};
    
    // Split full filename into filename and extension
    const char *dot_position = strchr(filename, '.');
    if (dot_position != NULL) {
        size_t filename_length = dot_position - filename;
        size_t extension_length = strlen(dot_position + 1);
        
        // Making sure filename and extension do not exceed
        if (filename_length > 8) filename_length = 8;
        if (extension_length > 3) extension_length = 3;
        
        strncpy(just_filename, filename, filename_length);
        strncpy(extension, dot_position + 1, extension_length);
    } else {
        // Extension not available
        strncpy(just_filename, filename, 8);
    }
    
    // Creating a new directory entry
    struct directory_entry new_entry;

    memset(&new_entry, 0, sizeof(struct directory_entry));
    strncpy((char *)new_entry.filename, just_filename, 8);
    strncpy((char *)new_entry.extension, extension, 3);
    new_entry.attributes = 0x20; // File attribute: Archive
    new_entry.first_logical_cluster = free_sectors[0];
    new_entry.file_size = file_size;

    // Get file statistics
    struct stat file_stat;
    if (fstat(fileno(source_file), &file_stat) != 0) {
        perror("Error retrieving file stats");
        exit(1);
    }

    // Convert times to FAT12 format
    struct tm *time_info = localtime(&file_stat.st_mtime);
    // Use temporary variables
    uint16_t temp_date, temp_time;
    convert_to_fat_time(time_info, &temp_date, &temp_time);

    // Assign converted values to the directory entry
    new_entry.creation_date = temp_date;
    new_entry.creation_time = temp_time;
    new_entry.last_write_date = temp_date;
    new_entry.last_write_time = temp_time;

    // Writing the new dir entry to the root
    if (fseek(disk, (9728 + (free_entry_index * 32)), SEEK_SET) != 0) {
        perror("Failed to seek to free entry in root dir");
        exit(EXIT_FAILURE);
    }

    if (fwrite(&new_entry, sizeof(struct directory_entry), 1, disk) != 1) {
        perror("Error writing directory entry to disk");
        exit(EXIT_FAILURE);
    }

    // Write the updated FAT
    size_t total_bytes_fat = boot_sector->sector_per_fat * boot_sector->bytes_per_sec;
    
    // Write FAT data to FAT1 location
    write_fat_to_disk(disk, 512, FAT, total_bytes_fat);

    // Write FAT data to FAT2 location
    write_fat_to_disk(disk, 5120, FAT, total_bytes_fat);

    if (fclose(source_file) != 0) {
        perror("Failed to close the source_file");
        exit(EXIT_FAILURE);
    }
}

// Write file to a subdirectory
void write_to_sub_dir(FILE *disk, uint8_t *FAT, struct fat_12_boot_sector *boot_sector, char *filename, uint16_t *found_cluster) {
    // Make sure 'found_cluster' is valid pointer
    if (found_cluster == NULL) {
        fprintf(stderr, "Error: 'found_cluster' is NULL.\n");
        exit(1);
    }

    uint16_t cluster = *found_cluster;
    int free_entry_found = 0;
    int free_entry_index = 0;

    char filename_copy[32];
    strncpy(filename_copy, filename, sizeof(filename_copy) - 1);
    filename_copy[sizeof(filename_copy) - 1] = '\0';

    int subdir_start = -1;
    while (cluster < 0xFF8) {
        // Seek to the start of the subdirectory
        subdir_start = ((33 + cluster - 2)* 512);
        if (fseek(disk, subdir_start, SEEK_SET) != 0) {
            perror("Failed to seek to the start of a sub dir");
            exit(EXIT_FAILURE);
        }

        // Iterating over all the entries in the given subdirectory
        for (int i = 0; i < 16; i++) {
            struct directory_entry entry;
            if (fread(&entry, sizeof(struct directory_entry), 1, disk) != 1) {
                perror("Failed to read directory entry");
                exit(EXIT_FAILURE);
            }

            // Flag that a free entry is available and get the logical cluster number
            if (entry.filename[0] == 0xE5 || entry.filename[0] == 0x00) {
                if (!free_entry_found) {
                    free_entry_found = 1;
                    free_entry_index = i;
                }
                continue;
            }

            // Check if the filename matches
            char entry_name[13];
            snprintf(entry_name, 13, "%.8s.%.3s", entry.filename, entry.extension);
            entry_name[12] = '\0';
            for (int j = 0; j < 12; j++) {
                if (entry_name[j] == ' ') entry_name[j] = '\0';
            }

            // Capitalizing the filename for comparison
            for (char *p = filename; *p; ++p) {
                *p = toupper((unsigned char)*p);
            }

            if (strcmp(entry_name, filename) == 0) {
                printf("Error: The file %s already exists in the subdirectory.\n", filename);
                exit(1);
            }
        }
        cluster = get_FAT_entry(FAT, cluster);
    }

    // If no free entry found in the subdirectory
    if (!free_entry_found) {
        printf("Error: No free entry available in the subdirectory.\n");
        exit(1);
    }

    // Open the source file to be copied
    FILE *source_file = fopen(filename_copy, "rb");
    if (!source_file) {
        printf("Error: File not found or cannot be opened.\n");
        exit(1);
    }

    // Get the file size
    if (fseek(source_file, 0, SEEK_END) != 0) {
        perror("Failed to seek to the end of the source file");
        exit(EXIT_FAILURE);
    }
    long file_size = ftell(source_file);

    // Check if ftell is successful
    if (file_size == -1L) {
        perror("Failed to get the file size");
        exit(EXIT_FAILURE);
    }

    if (fseek(source_file, 0, SEEK_SET) != 0) {
        perror("Failed to seek to the start of the source file");
        exit(EXIT_FAILURE);
    }

    // Calculate the number of sectors needed
    int sectors_needed = (file_size + boot_sector->bytes_per_sec - 1) / boot_sector->bytes_per_sec;
    int free_sectors[sectors_needed];
    mark_entries_in_fat(FAT, free_sectors, sectors_needed, boot_sector->total_sector_count - 33 + 2);

    // Write the file data to the disk image
    for (int i = 0; i < sectors_needed; i++) {
        uint8_t buffer[boot_sector->bytes_per_sec];
        size_t bytes_read = fread(buffer, 1, boot_sector->bytes_per_sec, source_file);

        // Check if fread succeeded in reading the expected number of bytes
        if (bytes_read != boot_sector->bytes_per_sec) {
            if (ferror(source_file)) {
                perror("Error reading from source file");
                exit(EXIT_FAILURE);
            }
        }

        if (fseek(disk, ( 33 + free_sectors[i] - 2) * boot_sector->bytes_per_sec, SEEK_SET) != 0) {
            perror("Failed to seek to certain data sector");
            exit(EXIT_FAILURE);
        }

        // Write data to the disk
        if (fwrite(buffer, 1, boot_sector->bytes_per_sec, disk) != boot_sector->bytes_per_sec) {
            perror("Error writing to disk");
            exit(EXIT_FAILURE);
        }
    }

    // Update the directory entry
    char just_filename[9] = {0};
    char extension[4] = {0};

    const char *dot_position = strchr(filename, '.');
    if (dot_position != NULL) {
        size_t filename_length = dot_position - filename;
        size_t extension_length = strlen(dot_position + 1);
        
        if (filename_length > 8) filename_length = 8;
        if (extension_length > 3) extension_length = 3;
        
        strncpy(just_filename, filename, filename_length);
        strncpy(extension, dot_position + 1, extension_length);
    } 
    else {
        strncpy(just_filename, filename, 8);
    }

    struct directory_entry new_entry;
    memset(&new_entry, 0, sizeof(struct directory_entry));

    strncpy((char *)new_entry.filename, just_filename, 8);
    strncpy((char *)new_entry.extension, extension, 3);
    new_entry.attributes = 0x20; // File attribute: Archive
    new_entry.first_logical_cluster = free_sectors[0];
    new_entry.file_size = file_size;

    // Get file statistics
    struct stat file_stat;
    if (fstat(fileno(source_file), &file_stat) != 0) {
        perror("Error retrieving file stats");
        exit(1);
    }

    // Convert times to FAT12 format
    struct tm *time_info = localtime(&file_stat.st_mtime);
    uint16_t temp_date, temp_time;
    convert_to_fat_time(time_info, &temp_date, &temp_time);

    new_entry.creation_date = temp_date;
    new_entry.creation_time = temp_time;
    new_entry.last_write_date = temp_date;
    new_entry.last_write_time = temp_time;

    if (fseek(disk, (subdir_start + (free_entry_index * 32)), SEEK_SET) != 0) {
        perror("Failed to seek to the free entry in sub dir");
        exit(EXIT_FAILURE);
    }

    if (fwrite(&new_entry, sizeof(struct directory_entry), 1, disk) != 1) {
        perror("Error writing directory entry to disk");
        exit(EXIT_FAILURE);
    }

    // Write the updated FAT
    size_t total_bytes_fat = boot_sector->sector_per_fat * boot_sector->bytes_per_sec;
    
    // Write FAT data to FAT1 location
    write_fat_to_disk(disk, 512, FAT, total_bytes_fat);

    // Write FAT data to FAT2 location
    write_fat_to_disk(disk, 5120, FAT, total_bytes_fat);

    if (fclose(source_file) != 0) {
        perror("Failed to close the source_file");
        exit(EXIT_FAILURE);
    }
}

// Copy the file to the disk from the local linux file system
void copy_file_to_image(const char *file_path, FILE *disk, uint8_t *FAT, struct fat_12_boot_sector *boot_sector) {
    char dir_path[MAX_PATH_LEN] = "";
    char file_name[MAX_PATH_LEN];

    // Extract the path and filename in the desired format
    extract_path_and_filename(file_path, dir_path, file_name);
    
    // Check if the source file exists in the current linux directory
    // If the file name is passed with disk directory path
    if (strlen(dir_path) > 0) {
        if (!file_exists(file_name)) {
            fprintf(stderr, "File not found: %s\n", file_name);
            // Close the disk
            if (fclose(disk) != 0) {
                perror("Failed to close the file");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_FAILURE);
        }
    }
    // If just the filename is passed (copy to root)
    else {
        if (!file_exists(file_path)) {
            fprintf(stderr, "File not found: %s\n", file_path);
            // Close the disk
            if (fclose(disk) != 0) {
                perror("Failed to close the file");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_FAILURE);
        }
    }

    // Check if the dir_path is mentioned along with the file and find its logical cluster
    uint16_t found_cluster = 0;
    if (strlen(dir_path) > 0) {
        // Get the first logical cluster of the directory entry
        get_first_logical_cluster_dir(disk, FAT, boot_sector, dir_path, &found_cluster);

        if (found_cluster == 0) {
            fprintf(stderr, "No directory found with path %s\n", dir_path);
            // Close the disk
            if (fclose(disk) != 0) {
                perror("Failed to close the file");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_FAILURE);
        }
    }

    // Calculate the free space on the disk
    int free_space_on_disk = 512 * count_free_sectors(FAT, (boot_sector->total_sector_count-33+2));
    off_t size;
    time_t creation_time, modification_time;

    // Get the file information
    get_file_info(file_name, &size, &creation_time, &modification_time);

    // Check if enough free space for the file
    if (size > free_space_on_disk) {
        fprintf(stderr, "No enough free space in the disk image\n");
        // Close the disk
        if (fclose(disk) != 0) {
            perror("Failed to close the file");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }

    // Writing to the disk
    // Write to the dir_path as specified with the file
    if (strlen(dir_path) > 0) {
        write_to_sub_dir(disk, FAT, boot_sector, file_name, &found_cluster);
    }
    // Write to the root directory
    else {
        write_to_root_dir(disk, FAT, boot_sector, file_name);
    }

    // Close the disk
    if (fclose(disk) != 0) {
        perror("Failed to close the file");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    // Check number of arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image file> <optional path with filename or filename only>\n", argv[0]);
        return 1;
    }

    // Open the disk
    FILE *disk = fopen(argv[1], "r+b");
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

    // Copying the file from the system to the disk
    copy_file_to_image(argv[2], disk, FAT, &boot_sector);

    free(FAT);
    printf("Success!!\n");
    return 0;
}