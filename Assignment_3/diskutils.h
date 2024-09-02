#ifndef DISKUTILS_H
#define DISKUTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Structure for reading boot sector information
struct fat_12_boot_sector {
    uint8_t ignore_1[3];
    char os_name[8];
    uint16_t bytes_per_sec;
    uint8_t sectors_per_cluster;
    uint16_t num_reserved_sector;
    uint8_t num_fat;
    uint16_t max_num_root_dirs;
    uint16_t total_sector_count;
    uint8_t ignore_2;
    uint16_t sector_per_fat;
    uint16_t sector_per_track;
    uint16_t num_heads;
    uint32_t ignore_3;
    uint32_t total_sec_cnt_fat32;
    uint16_t ignore_4;
    uint8_t boot_signature;
    uint32_t vol_id;
    char vol_label[11];
} __attribute__((__packed__));

// Structure for reading the directory entry
struct directory_entry {
    unsigned char filename[8];
    unsigned char extension[3];
    uint8_t attributes;
    uint16_t reserved;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t ignore;
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_logical_cluster;
    uint32_t file_size;
} __attribute__((__packed__));

// Function prototype for reading the boot sector
void read_boot_sector(FILE *disk, struct fat_12_boot_sector *boot_sector);

// Function prototype for reading the FAT
void read_fat(FILE *disk, struct fat_12_boot_sector *boot_sector, uint8_t *FAT);

// Function prototype for getting the fat entry
int get_FAT_entry(uint8_t *FAT, int n);

#endif // DISKUTILS_H