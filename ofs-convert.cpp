#include "fat.h"
#include "partition.h"
#include "stream-archiver.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint64_t pageSize;

struct cluster_read_state {
    struct extent current_extent;
    uint32_t extent_cluster;
    struct fat_dentry *current_cluster;
    uint32_t cluster_dentry;
};

struct fat_dentry *next_dentry(StreamArchiver *extent_stream, struct cluster_read_state *state) {
    struct fat_dentry *ret;
    do {
        state->cluster_dentry++;
        if (state->cluster_dentry < meta_info.dentries_per_cluster) {
            ret = state->current_cluster + state->cluster_dentry;
        } else {
            state->extent_cluster++;
            if (state->extent_cluster < state->current_extent.length) {
                uint32_t cluster_no = state->current_extent.physical_start + state->extent_cluster;
                state->current_cluster = (struct fat_dentry *) cluster_start(cluster_no);
                state->cluster_dentry = 0;
                ret = state->current_cluster;
            } else {
                state->current_extent = *(struct extent *) iterateStreamArchiver(extent_stream, false, sizeof(struct extent));
                state->extent_cluster = 0;
                uint32_t cluster_no = state->current_extent.physical_start + state->extent_cluster;
                state->current_cluster = (struct fat_dentry *) cluster_start(cluster_no);
                state->cluster_dentry = 0;
                ret = state->current_cluster;
            }
        }
    } while (is_invalid(ret) || is_dot_dir(ret));
    return ret;
}

struct fat_dentry *init_cluster_read_state(StreamArchiver *extent_stream, struct cluster_read_state *state) {
    state->current_extent = *(struct extent *) iterateStreamArchiver(extent_stream, false, sizeof(struct extent));
    state->extent_cluster = 0;
    uint32_t cluster_no = state->current_extent.physical_start + state->extent_cluster;
    state->current_cluster = (struct fat_dentry *) cluster_start(cluster_no);
    state->cluster_dentry = -1;  // dummy value that next_dentry will increment to 0
    return next_dentry(extent_stream, state);
}

void reserve_name(uint16_t *pointers[], int count, StreamArchiver *write_stream) {
    for (int i = 0; i < count; i++) {
        pointers[i] = (uint16_t *) iterateStreamArchiver(write_stream, true, LFN_ENTRY_LENGTH * sizeof(uint16_t));
    }
    cutStreamArchiver(write_stream);
}

struct fat_dentry *reserve_dentry(StreamArchiver *write_stream) {
    void *p = iterateStreamArchiver(write_stream, true, sizeof(struct fat_dentry));
    cutStreamArchiver(write_stream);
    return (struct fat_dentry *) p;
}

struct extent *reserve_extent(StreamArchiver *write_stream) {
    void *p = iterateStreamArchiver(write_stream, true, sizeof(struct extent));
    cutStreamArchiver(write_stream);
    return (struct extent *) p;
}

uint32_t *reserve_children_count(StreamArchiver *write_stream) {
    void *p = iterateStreamArchiver(write_stream, true, sizeof(uint32_t));
    cutStreamArchiver(write_stream);
    return (uint32_t *) p;
}

void read_extents(uint32_t cluster_no, StreamArchiver *write_stream) {
    struct extent *current_extent = reserve_extent(write_stream);
    *current_extent = {0, 1, cluster_no};

    uint32_t next_cluster_no = *fat_entry(cluster_no);

    while (next_cluster_no < FAT_END_OF_CHAIN) {
        if (next_cluster_no == current_extent->physical_start + current_extent->length) {
            current_extent->length++;
        } else {
            struct extent new_extent = {current_extent->logical_start + current_extent->length, 1, next_cluster_no};
            current_extent = reserve_extent(write_stream);
            *current_extent = new_extent;
        }
        next_cluster_no = *fat_entry(next_cluster_no);
    }
}

struct fat_dentry *read_lfn(struct fat_dentry *first_entry, StreamArchiver *extent_stream, uint16_t *name[], uint8_t lfn_entry_count, struct cluster_read_state *state) {
    uint8_t *entry = (uint8_t *) first_entry;
    for (int i = lfn_entry_count - 1; i >= 0; i--) {
        lfn_cpy(name[i], entry);
        entry = (uint8_t *) next_dentry(extent_stream, state);
    }
    return (struct fat_dentry *) entry;
}

void traverse(StreamArchiver *dir_extent_stream, StreamArchiver *write_stream) {
    struct cluster_read_state state = {};

    uint32_t *children_count = reserve_children_count(write_stream);
    struct fat_dentry *current_dentry = init_cluster_read_state(dir_extent_stream, &state);

    while (!is_dir_table_end(current_dentry)) {
        bool has_long_name = is_lfn(current_dentry);
        if (has_long_name) {
            uint8_t lfn_entry_count = lfn_entry_sequence_no(current_dentry);
            uint16_t *name[lfn_entry_count];
            reserve_name(name, lfn_entry_count, write_stream);
            current_dentry = read_lfn(current_dentry, dir_extent_stream, name, lfn_entry_count, &state);
        } else {
            uint16_t *name[1];
            reserve_name(name, 1, write_stream);
            read_short_name(current_dentry, name[0]);
        }

        struct fat_dentry *dentry = reserve_dentry(write_stream);
        memcpy(dentry, current_dentry, sizeof *current_dentry);

        uint32_t cluster_no = file_cluster_no(current_dentry);
        StreamArchiver read_extent_stream = *write_stream;
        read_extents(cluster_no, write_stream);
        if (is_dir(current_dentry)) {
            traverse(&read_extent_stream, write_stream);
        } else {
            *reserve_children_count(write_stream) = -1;
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

    StreamArchiver stream;
    init_stream_archiver(&stream);
    StreamArchiver ext_stream = stream;
    read_extents(boot_sector.root_cluster_no, &stream);
    traverse(&ext_stream, &stream);
}
