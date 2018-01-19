#include "fat.h"
#include "partition.h"
#include "stream-archiver.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dentries_per_cluster = meta_info.cluster_size / sizeof(struct fat_dentry);
extern uint64_t pageSize;

struct read_state {
    struct extent current_extent;
    uint32_t extent_cluster;
    struct fat_dentry *current_cluster;
    int cluster_dentry;
};

struct fat_dentry *init_read_state(StreamArchiver *extent_stream, struct read_state *state) {
    state->current_extent = *(struct extent *) iterateStreamArchiver(extent_stream, false, sizeof(struct extent));
    state->extent_cluster = 0;
    uint32_t cluster_no = state->current_extent.physical_start + state->extent_cluster;
    state->current_cluster = (struct fat_dentry *) cluster_start(cluster_no);
    state->cluster_dentry = 0;
    return state->current_cluster;
}

struct fat_dentry *next_dentry(StreamArchiver *extent_stream, struct read_state *state) {
    state->cluster_dentry++;
    if (state->cluster_dentry < dentries_per_cluster) {
        return state->current_cluster + state->cluster_dentry;
    }

    state->extent_cluster++;
    if (state->extent_cluster < state->current_extent.length) {
        uint32_t cluster_no = state->current_extent.physical_start + state->extent_cluster;
        state->current_cluster = (struct fat_dentry *) cluster_start(cluster_no);
        state->cluster_dentry = 0;
        return state->current_cluster;
    }

    state->current_extent = *(struct extent *) iterateStreamArchiver(extent_stream, false, sizeof(struct extent));
    state->extent_cluster = 0;
    uint32_t cluster_no = state->current_extent.physical_start + state->extent_cluster;
    state->current_cluster = (struct fat_dentry *) cluster_start(cluster_no);
    state->cluster_dentry = 0;
    return state->current_cluster;
}


struct extent *reserve_extent(StreamArchiver *write_stream) {
    void *p = iterateStreamArchiver(write_stream, true, sizeof(struct extent));
    cutStreamArchiver(write_stream);
    return (struct extent *) p;
}

void read_extents(uint32_t cluster_no, StreamArchiver *write_stream) {
    struct extent *current_extent = reserve_extent(write_stream);
    *current_extent = {0, 1, cluster_no};

    uint32_t next_cluster_no = *(uint32_t*) fat_entry(cluster_no);

    while (next_cluster_no < FAT_END_OF_CHAIN) {
        if (next_cluster_no == current_extent->physical_start + current_extent->length) {
            current_extent->length++;
        } else {
            struct extent new_extent = {current_extent->logical_start + current_extent->length, 1, next_cluster_no};
            current_extent = reserve_extent(write_stream);
            *current_extent = new_extent;
        }
        next_cluster_no = *(uint32_t*) fat_entry(next_cluster_no);
    }
}

int *reserve_children_count(StreamArchiver *write_stream) {
    void *p = iterateStreamArchiver(write_stream, true, sizeof(int));
    cutStreamArchiver(write_stream);
    return (int *) p;
}

uint16_t *reserve_name(int count, StreamArchiver *write_stream) {
    void *p = iterateStreamArchiver(write_stream, true, count * LFN_ENTRY_LENGTH * sizeof(uint16_t));
    cutStreamArchiver(write_stream);
    return (uint16_t *) p;
}

struct fat_dentry *reserve_dentry(StreamArchiver *write_stream) {
    void *p = iterateStreamArchiver(write_stream, true, sizeof(struct fat_dentry));
    cutStreamArchiver(write_stream);
    return (struct fat_dentry *) p;
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

struct fat_dentry *read_lfn(struct fat_dentry *first_entry, StreamArchiver *extent_stream, uint16_t *name, int lfn_entry_count, struct read_state *state) {
    char *entry = (char *) first_entry;
    for (int i = lfn_entry_count - 1; i >= 0; i--) {
        char *name_part = entry + i * LFN_ENTRY_LENGTH * sizeof(uint16_t);
        memcpy(name, name_part + 1, 5 * sizeof(uint16_t));
        memcpy(name + 5, name_part + 14, 6 * sizeof(uint16_t));
        memcpy(name + 11, name_part + 28, 2 * sizeof(uint16_t));

        entry = (char *) next_dentry(extent_stream, state);
    }
    return (struct fat_dentry *) entry;
}

// TODO types
void traverse(StreamArchiver *dir_extent_stream, StreamArchiver *write_stream) {
    struct read_state state = {};

    int *children_count = reserve_children_count(write_stream);
    struct fat_dentry *current_dentry = init_read_state(dir_extent_stream, &state);
    while (is_invalid(current_dentry) || is_dot_dir(current_dentry)) {
        current_dentry = next_dentry(dir_extent_stream, &state);
    }

    while (!is_dir_table_end(current_dentry)) {
        uint16_t *name;
        bool has_long_name = is_lfn(current_dentry);
        if (has_long_name) {
            int lfn_entry_count = lfn_entry_sequence_no(current_dentry);
            name = reserve_name(lfn_entry_count, write_stream);
            current_dentry = read_lfn(current_dentry, dir_extent_stream, name, lfn_entry_count, &state);
        } else {
            name = reserve_name(1, write_stream);
        }

        struct fat_dentry *dentry = reserve_dentry(write_stream);
        memcpy(dentry, current_dentry, sizeof *current_dentry);

        if (!has_long_name) {
            read_short_name(current_dentry, name);
        }

        for (int i = 0; i < 20 && name[i] != 0; i++) {
            putc(name[i], stdout);
        }
        putc('\n', stdout);

        uint32_t cluster_no = file_cluster_no(current_dentry);
        StreamArchiver read_extent_stream = *write_stream;
        read_extents(cluster_no, write_stream);
        if (is_dir(current_dentry)) {
            traverse(&read_extent_stream, write_stream);
        }

        (*children_count)++;
        current_dentry = next_dentry(dir_extent_stream, &state);

        while (is_invalid(current_dentry) || is_dot_dir(current_dentry)) {
            current_dentry = next_dentry(dir_extent_stream, &state);
        }
    }
}

void init_stream_archiver(StreamArchiver *stream) {
    pageSize = 32;
    memset(stream, 0, sizeof *stream);
    cutStreamArchiver(stream);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Wrong usage\n");
        exit(1);
    }
    Partition partition = {.path = argv[1]};
    if (!openPartition(&partition)) {
        fprintf(stderr, "Failed to open partition");
        return 1;
    }

    read_boot_sector(partition.ptr);
    set_meta_info(partition.ptr);
    dentries_per_cluster = meta_info.cluster_size / sizeof(struct fat_dentry);

    StreamArchiver stream;
    init_stream_archiver(&stream);
    StreamArchiver ext_stream = stream;
    read_extents(boot_sector.root_cluster_no, &stream);
    traverse(&ext_stream, &stream);
}
