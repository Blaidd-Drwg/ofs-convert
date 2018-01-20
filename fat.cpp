#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <stdint.h>
#include <stdbool.h>

#include "fat.h"
#include "partition.h"

TAILQ_HEAD(extent_list, extent_lentry);

struct boot_sector boot_sector;
struct meta_info meta_info;

struct extent_lentry {
    TAILQ_ENTRY(extent_lentry) entries;
    struct extent extent;
};

uint32_t *fat_entry(uint32_t cluster_no) {
    return meta_info.fat_start + cluster_no;
}

uint8_t *cluster_start(uint32_t cluster_no) {
    return meta_info.data_start + (cluster_no - FAT_START_INDEX) * meta_info.cluster_size;
}

bool is_free_cluster(uint32_t cluster_entry) {
    return (cluster_entry & CLUSTER_ENTRY_MASK) == FREE_CLUSTER;
}

uint32_t file_cluster_no(struct fat_dentry *dentry) {
    uint16_t low = dentry->first_cluster_low;
    uint32_t high = dentry->first_cluster_high << 16;
    return high | low;
}

bool is_dir(struct fat_dentry *dentry) {
    return dentry->attrs & 0x10;
}

bool is_lfn(struct fat_dentry *dentry) {
    return dentry->attrs & 0x0F;
}

bool is_invalid(struct fat_dentry *dentry) {
    return *(uint8_t *) dentry == 0xE5;
}

bool is_dir_table_end(struct fat_dentry *dentry) {
    return *(uint8_t *) dentry == 0x00;
}

bool is_dot_dir(struct fat_dentry *dentry) {
    return dentry->short_name[0] == '.';
}

bool is_last_lfn_entry(struct fat_dentry *dentry) {
    return *(uint8_t *) dentry & 0x40;
}

bool has_lower_name(struct fat_dentry *dentry) {
    return dentry->short_name_case & 0x8;
}

bool has_lower_extension(struct fat_dentry *dentry) {
    return dentry->short_name_case & 0x10;
}

bool has_extension(struct fat_dentry *dentry) {
    return dentry->short_extension[0] != ' ';
}

void lfn_cpy(uint16_t *dest, uint8_t *src) {
    memcpy(dest, src + 1, 5 * sizeof(uint16_t));
    memcpy(dest + 5, src + 14, 6 * sizeof(uint16_t));
    memcpy(dest + 11, src + 28, 2 * sizeof(uint16_t));
}

uint8_t lfn_entry_sequence_no(struct fat_dentry *dentry) {
    return *(uint8_t *) dentry & 0x1F;
}

void read_short_name(struct fat_dentry *dentry, uint16_t *name) {
    bool lower_name = has_lower_name(dentry);
    bool lower_extension = has_lower_extension(dentry);

    uint8_t *n = dentry->short_name;
    for (int i = 0; i < 8 && n[i] != ' '; i++) {
        *name = lower_name ? tolower(n[i]) : n[i];
        name++;
    }

    if (has_extension(dentry)) {
        *name = '.';
        name++;

        uint8_t *e = dentry->short_extension;
        for (int i = 0; i < 3 && e[i] != ' '; i++) {
            *name = lower_extension ? tolower(e[i]) : e[i];
            name++;
        }
    }
    *name = 0;
}

void read_boot_sector(uint8_t *fs) {
    boot_sector = *(struct boot_sector*) fs;
}

void set_meta_info(uint8_t *fs) {
    meta_info.fs_start = fs;
    meta_info.fat_start = (uint32_t *) (fs + boot_sector.sectors_before_fat * boot_sector.bytes_per_sector);
    meta_info.fat_entries = boot_sector.sectors_per_fat / boot_sector.sectors_per_cluster;
    meta_info.cluster_size = boot_sector.sectors_per_cluster * boot_sector.bytes_per_sector;
    meta_info.dentries_per_cluster = meta_info.cluster_size / sizeof(struct fat_dentry);
    meta_info.data_start = (uint8_t*) meta_info.fat_start + boot_sector.fat_count * boot_sector.sectors_per_fat * boot_sector.bytes_per_sector;
}
