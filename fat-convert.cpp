#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <stdint.h>
#include <stdbool.h>

#include "fat.h"
#include "partition.h"

uint32_t END_OF_CHAIN = 0xFFFFFF8;
TAILQ_HEAD(listhead, extent_lentry);

struct boot_sector boot_sector;
struct meta_info meta_info;

struct extent {
	uint32_t logical_start;  // First file cluster number that this extent covers
	uint32_t length;  // Number of clusters covered by extent
	uint32_t physical_start;  // Physical cluster number to which this extent points
};

struct extent_lentry {
	TAILQ_ENTRY(extent_lentry) entries;
	struct extent extent;
};

bool is_dir(struct fat_dentry *dentry) {
	return dentry->attrs & 0x10;
}

bool is_long_name(struct fat_dentry *dentry) {
	return dentry->attrs & 0x0F;
}

bool is_invalid(struct fat_dentry *dentry) {
	return dentry->short_name[0] == 0xE5;
}

bool is_dir_table_end(struct fat_dentry *dentry) {
	return dentry->short_name[0] == 0x00;
}

bool is_dot_dir(struct fat_dentry *dentry) {
	return dentry->short_name[0] == '.';
}

struct listhead read_extents(uint32_t cluster_no) {
	struct listhead head;
	TAILQ_INIT(&head);
	struct extent current_extent = {0, 1, cluster_no};

	uint32_t next_cluster_no = *(uint32_t*) fat_entry(cluster_no);

	while (next_cluster_no < END_OF_CHAIN) {
		if (next_cluster_no == current_extent.physical_start + current_extent.length) {
			current_extent.length++;
		} else {
			struct extent_lentry *new_lentry = (struct extent_lentry *) malloc(sizeof(struct extent_lentry));
			new_lentry->extent = current_extent;
			TAILQ_INSERT_TAIL(&head, new_lentry, entries);

			struct extent new_extent = {current_extent.logical_start + current_extent.length, 1, next_cluster_no};
			current_extent = new_extent;
		}
		next_cluster_no = *(uint32_t*) fat_entry(next_cluster_no);
	}
	struct extent_lentry *new_lentry = (struct extent_lentry *) malloc(sizeof(struct extent_lentry));
	new_lentry->extent = current_extent;
	TAILQ_INSERT_TAIL(&head, new_lentry, entries);
	return head;
}

uint8_t *read_dir(struct listhead extents_list) {
	size_t dir_size = 0;
	for (struct extent_lentry *it = extents_list.tqh_first; it != NULL; it = it->entries.tqe_next) {
		dir_size += it->extent.length;
	}

	uint8_t *dir_data = (uint8_t*) malloc(dir_size * meta_info.cluster_size);
	for (struct extent_lentry *it = extents_list.tqh_first; it != NULL; it = it->entries.tqe_next) {
		struct extent current_extent = it->extent;
		memcpy(dir_data + current_extent.logical_start,
		       cluster_start(current_extent.physical_start),
		       meta_info.cluster_size * current_extent.length);
	}
	return dir_data;
}

void recursive_traverse(uint32_t cluster_no) {
	struct listhead root_extents = read_extents(cluster_no);
	uint8_t *root_data = read_dir(root_extents);

	struct fat_dentry *current_dentry = (struct fat_dentry *) root_data;
	for (; !is_dir_table_end(current_dentry); current_dentry++) {
		if (!is_invalid(current_dentry) && !is_long_name(current_dentry)) {
			if (is_dir(current_dentry)) {
				if (!is_dot_dir(current_dentry)) {
					uint16_t low = current_dentry->first_cluster_low;
					uint32_t high = current_dentry->first_cluster_high << 16;
					uint32_t dir_cluster_no = high | low;
					printf("dir: %.8s\n", current_dentry->short_name);
					recursive_traverse(dir_cluster_no);
				}
			} else {
				printf("file: %.8s\n", current_dentry->short_name);
			}
		}
	}
}

void read_boot_sector(uint8_t *fs) {
	boot_sector = *(struct boot_sector*) fs;
}

void set_meta_info(uint8_t *fs) {
	meta_info.fs_start = fs;
	meta_info.fat_start = fs + boot_sector.sectors_before_fat * boot_sector.bytes_per_sector;
	meta_info.fat_entries = boot_sector.sectors_per_fat / boot_sector.sectors_per_cluster;
	meta_info.cluster_size = boot_sector.sectors_per_cluster * boot_sector.bytes_per_sector;
	meta_info.data_start = meta_info.fat_start + boot_sector.fat_count * boot_sector.sectors_per_fat * boot_sector.bytes_per_sector;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Wrong usage\n");
		exit(1);
	}
	Partition partition = {.path = argv[1]};
	openPartition(&partition);
	read_boot_sector(partition.ptr);
	set_meta_info(partition.ptr);
	recursive_traverse(boot_sector.root_cluster_no);
}
