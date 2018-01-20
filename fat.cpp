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

uint8_t *fat_entry(uint32_t cluster_no) {
    return meta_info.fat_start + cluster_no * 4;
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

void lfn_cpy(uint16_t *dest, struct fat_dentry *src_dentry, uint8_t sequence_no) {
    uint16_t *dest_start = dest + (sequence_no - 1) * LFN_ENTRY_LENGTH;
    uint8_t *src = (uint8_t *) src_dentry;
    memcpy(dest_start, src + 1, 10);
    memcpy(dest_start + 5, src + 14, 12);
    memcpy(dest_start + 11, src + 28, 4);
}

uint8_t lfn_entry_sequence_no(struct fat_dentry *dentry) {
    return *(uint8_t *) dentry & 0x1F;
}

struct extent_list read_extents(uint32_t cluster_no) {
    struct extent_list head;
    TAILQ_INIT(&head);
    struct extent current_extent = {0, 1, cluster_no};

    uint32_t next_cluster_no = *(uint32_t*) fat_entry(cluster_no);

    while (next_cluster_no < FAT_END_OF_CHAIN) {
        if (next_cluster_no == current_extent.physical_start + current_extent.length) {
            current_extent.length++;
        } else {
            struct extent_lentry *new_lentry = (struct extent_lentry *) malloc(sizeof *new_lentry);
            new_lentry->extent = current_extent;
            TAILQ_INSERT_TAIL(&head, new_lentry, entries);

            struct extent new_extent = {current_extent.logical_start + current_extent.length, 1, next_cluster_no};
            current_extent = new_extent;
        }
        next_cluster_no = *(uint32_t*) fat_entry(next_cluster_no);
    }
    struct extent_lentry *new_lentry = (struct extent_lentry *) malloc(sizeof *new_lentry);
    new_lentry->extent = current_extent;
    TAILQ_INSERT_TAIL(&head, new_lentry, entries);
    return head;
}

uint8_t *read_dir(uint32_t cluster_no) {
    struct extent_list extents = read_extents(cluster_no);

    size_t dir_size = 0;
    for (struct extent_lentry *it = extents.tqh_first; it != NULL; it = it->entries.tqe_next) {
        dir_size += it->extent.length;
    }

    uint8_t *dir_data = (uint8_t*) malloc(dir_size * meta_info.cluster_size);
    for (struct extent_lentry *it = extents.tqh_first; it != NULL; it = it->entries.tqe_next) {
        struct extent current_extent = it->extent;
        memcpy(dir_data + current_extent.logical_start,
               cluster_start(current_extent.physical_start),
               meta_info.cluster_size * current_extent.length);
    }
    return dir_data;
}

uint16_t *parse_long_name(struct fat_dentry *dentry, uint16_t *long_name) {
    uint8_t lfn_sequence_no = lfn_entry_sequence_no(dentry);
    if (is_last_lfn_entry(dentry)) {
        int max_name_length = LFN_ENTRY_LENGTH * lfn_sequence_no + 1;
        long_name = (uint16_t *) malloc(max_name_length * sizeof(uint16_t));
        long_name[max_name_length - 1] = 0;
    }
    lfn_cpy(long_name, dentry, lfn_sequence_no);
    return long_name;
}

void recursive_traverse(uint32_t cluster_no, uint16_t *long_name) {
    uint8_t *dir_data = read_dir(cluster_no);

    struct fat_dentry *current_dentry = (struct fat_dentry *) dir_data;
    for (; !is_dir_table_end(current_dentry); current_dentry++) {
        if (!is_invalid(current_dentry)) {
            if (is_lfn(current_dentry)) {
                long_name = parse_long_name(current_dentry, long_name);
            } else if (is_dir(current_dentry)) {
                if (!is_dot_dir(current_dentry)) {
                    printf("dir: %.8s\n", current_dentry->short_name);
                    recursive_traverse(file_cluster_no(current_dentry), long_name);
                }
            } else {
                printf("file: %.8s\n", current_dentry->short_name);
            }
        }
    }
    free(dir_data);
}

void read_boot_sector(uint8_t *fs) {
    boot_sector = *(struct boot_sector*) fs;
}

void set_meta_info(uint8_t *fs) {
    meta_info.fs_start = fs;
    meta_info.fat_start = fs + boot_sector.sectors_before_fat * boot_sector.bytes_per_sector;
    meta_info.fat_entries = boot_sector.sectors_per_fat / boot_sector.sectors_per_cluster;
    meta_info.cluster_size = boot_sector.sectors_per_cluster * boot_sector.bytes_per_sector;
    meta_info.dentries_per_cluster = meta_info.cluster_size / sizeof(struct fat_dentry);
    meta_info.data_start = meta_info.fat_start + boot_sector.fat_count * boot_sector.sectors_per_fat * boot_sector.bytes_per_sector;
}
