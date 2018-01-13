#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <stdint.h>
#include <stdbool.h>

uint32_t END_OF_CHAIN = 0xFFFFFF8;
TAILQ_HEAD(listhead, extent_lentry);

struct extent {
	uint32_t logical_start;  // First file cluster number that this extent covers
	uint32_t length;  // Number of clusters covered by extent
	uint32_t physical_start;  // Physical cluster number to which this extent points
};

struct extent_lentry {
	TAILQ_ENTRY(extent_lentry) entries;
	struct extent extent;
};

struct fat_dentry {
	uint8_t short_name[8];
	uint8_t short_extension[3];
	uint8_t attrs;
	uint8_t long_name_case;
	uint8_t create_time_10_ms;
	uint16_t create_time;
	uint16_t create_date;
	uint16_t access_date;
	uint16_t first_cluster_high;
	uint16_t mod_time;
	uint16_t mod_date;
	uint16_t first_cluster_low;
	uint32_t file_size;
};

struct __attribute__((packed)) boot_sector {
	uint8_t jump_instruction[3];
	uint8_t oem_name[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t sectors_before_fat;
	uint8_t fat_count;
	uint16_t dir_entries;
	uint16_t sector_count;
	uint8_t media_descriptor;
	uint16_t unused2;
	uint16_t sectors_per_disk_track;
	uint16_t disk_heads;
	uint32_t hidden_sectors_before_partition;
	uint32_t total_sectors2;  // used if total_sectors would have overflown;
	uint32_t sectors_per_fat;
	uint16_t drive_description_flags;
	uint16_t version;
	uint32_t root_cluster_no;
	uint16_t fs_info_sector_no;
	uint16_t backup_boot_sector_no;
	uint8_t reserved[12];
	uint8_t physical_drive_no;
	uint8_t reserved2;
	uint8_t ext_boot_signature;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint64_t fs_type;
};

struct meta_info {
	char *fat_start;
	uint16_t fat_entries;
	uint32_t cluster_size;
	char *data_start;
};

struct meta_info MetaInfo;
struct boot_sector BootSector;

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

uint64_t fat_entry(uint32_t cluster_no) {
	return MetaInfo.fat_start + cluster_no * 4;
}

uint64_t cluster_start(uint32_t cluster_no) {
	return MetaInfo.data_start + (cluster_no - 2) * MetaInfo.cluster_size;
}

struct listhead read_extents(uint32_t cluster_no, FILE *fs) {
	struct listhead head;
	TAILQ_INIT(&head);
	struct extent current_extent = {0, 1, cluster_no};

	char buf[4];
	fseek(fs, fat_entry(cluster_no), SEEK_SET);
	fread(buf, 1, 4, fs);
	uint32_t next_cluster_no = *(uint32_t*) buf;

	while (next_cluster_no < END_OF_CHAIN) {
		if (next_cluster_no == current_extent.physical_start + current_extent.length) {
			current_extent.length++;
		} else {
			struct extent_lentry *new_lentry = (struct extent_lentry *) malloc(sizeof(struct extent_lentry));
			new_lentry->extent = current_extent;
			TAILQ_INSERT_TAIL(&head, new_lentry, entries);

			struct extent new_extent = {current_extent.logical_start + current_extent.length, 1, next_cluster_no};
			current_extent = new_extent;
			fseek(fs, fat_entry(next_cluster_no), SEEK_SET);
		}
		fread(buf, 1, 4, fs);
		next_cluster_no = *(uint32_t*) buf;
	}
	struct extent_lentry *new_lentry = (struct extent_lentry *) malloc(sizeof(struct extent_lentry));
	new_lentry->extent = current_extent;
	TAILQ_INSERT_TAIL(&head, new_lentry, entries);
	return head;
}

char *read_dir(struct listhead extents_list, FILE *fs) {
	size_t dir_size = 0;
	for (struct extent_lentry *it = extents_list.tqh_first; it != NULL; it = it->entries.tqe_next) {
		dir_size += it->extent.length;
	}

	char *dir_data = (char*) malloc(dir_size * MetaInfo.cluster_size);
	for (struct extent_lentry *it = extents_list.tqh_first; it != NULL; it = it->entries.tqe_next) {
		struct extent current_extent = it->extent;
		fseek(fs, cluster_start(current_extent.physical_start), SEEK_SET);
		fread(dir_data + current_extent.logical_start, MetaInfo.cluster_size, current_extent.length, fs);
	}
	return dir_data;
}

void recursive_traverse(uint32_t cluster_no, FILE *fs) {
	struct listhead root_extents = read_extents(cluster_no, fs);
	char *root_data = read_dir(root_extents, fs);

	struct fat_dentry *current_dentry = (struct fat_dentry *) root_data;
	for (; !is_dir_table_end(current_dentry); current_dentry++) {
		if (!is_invalid(current_dentry) && !is_long_name(current_dentry)) {
			if (is_dir(current_dentry)) {
				if (!is_dot_dir(current_dentry)) {
					uint16_t low = current_dentry->first_cluster_low;
					uint32_t high = current_dentry->first_cluster_high << 16;
					uint32_t dir_cluster_no = high | low;
					printf("dir: %.8s\n", current_dentry->short_name);
					recursive_traverse(dir_cluster_no, fs);
				}
			} else {
				printf("file: %.8s\n", current_dentry->short_name);
			}
		}
	}
}

void read_boot_sector(FILE *fs) {
	fread(&BootSector, sizeof(struct boot_sector), 1, fs);
}

void set_meta_info() {
	MetaInfo.fat_start = BootSector.sectors_before_fat * BootSector.bytes_per_sector;
	MetaInfo.fat_entries = BootSector.sectors_per_fat / BootSector.sectors_per_cluster;
	MetaInfo.cluster_size = BootSector.sectors_per_cluster * BootSector.bytes_per_sector;
	MetaInfo.data_start = MetaInfo.fat_start + BootSector.fat_count * BootSector.sectors_per_fat * BootSector.bytes_per_sector;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Wrong usage\n");
		exit(1);
	}
	FILE *fs = fopen(argv[1], "r");
	read_boot_sector(fs);
	set_meta_info();
	recursive_traverse(BootSector.root_cluster_no, fs);
}
