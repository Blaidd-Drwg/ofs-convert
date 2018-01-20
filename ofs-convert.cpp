#include "fat.h"
#include "partition.h"
#include "stream-archiver.h"
#include "extent-allocator.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint64_t pageSize;

struct cluster_read_state {
    extent* current_extent;
    uint32_t extent_cluster;
    fat_dentry* current_cluster;
    uint32_t cluster_dentry;
};

fat_dentry* next_dentry(StreamArchiver* extent_stream, cluster_read_state* state) {
    fat_dentry* ret;
    do {
        state->cluster_dentry++;
        if (state->cluster_dentry < meta_info.dentries_per_cluster) {
            ret = state->current_cluster + state->cluster_dentry;
        } else {
            state->extent_cluster++;
            if (state->extent_cluster < state->current_extent->length) {
                uint32_t cluster_no = state->current_extent->physical_start + state->extent_cluster;
                state->current_cluster = reinterpret_cast<fat_dentry*>(cluster_start(cluster_no));
                state->cluster_dentry = 0;
                ret = state->current_cluster;
            } else {
                state->current_extent = reinterpret_cast<extent*>(iterateStreamArchiver(extent_stream, false, sizeof(extent)));
                state->extent_cluster = 0;
                uint32_t cluster_no = state->current_extent->physical_start + state->extent_cluster;
                state->current_cluster = reinterpret_cast<fat_dentry*>(cluster_start(cluster_no));
                state->cluster_dentry = 0;
                ret = state->current_cluster;
            }
        }
    } while (is_invalid(ret) || is_dot_dir(ret));
    return ret;
}

fat_dentry* init_cluster_read_state(StreamArchiver* extent_stream, cluster_read_state* state) {
    state->current_extent = reinterpret_cast<extent*>(iterateStreamArchiver(extent_stream, false, sizeof(extent)));
    state->extent_cluster = 0;
    uint32_t cluster_no = state->current_extent->physical_start + state->extent_cluster;
    state->current_cluster = reinterpret_cast<fat_dentry*>(cluster_start(cluster_no));
    state->cluster_dentry = -1;  // dummy value that next_dentry will increment to 0
    return next_dentry(extent_stream, state);
}

void reserve_name(uint16_t* pointers[], int count, StreamArchiver* write_stream) {
    for (int i = 0; i < count; i++) {
        pointers[i] = reinterpret_cast<uint16_t*>(iterateStreamArchiver(write_stream, true, LFN_ENTRY_LENGTH * sizeof(uint16_t)));
    }
    cutStreamArchiver(write_stream);
}

fat_dentry* reserve_dentry(StreamArchiver* write_stream) {
    void* ptr = iterateStreamArchiver(write_stream, true, sizeof(fat_dentry));
    cutStreamArchiver(write_stream);
    return reinterpret_cast<fat_dentry*>(ptr);
}

extent* reserve_extent(StreamArchiver* write_stream) {
    void* ptr = iterateStreamArchiver(write_stream, true, sizeof(extent));
    cutStreamArchiver(write_stream);
    return reinterpret_cast<extent*>(ptr);
}

uint32_t* reserve_children_count(StreamArchiver* write_stream) {
    void* ptr = iterateStreamArchiver(write_stream, true, sizeof(uint32_t));
    cutStreamArchiver(write_stream);
    return reinterpret_cast<uint32_t*>(ptr);
}

void resettle_extent(extent& input_extent, StreamArchiver* write_stream) {
    for(uint32_t i = 0; i < input_extent.length; ) {
        extent fragment = allocate_extent(input_extent.length - i);
        fragment.logical_start = input_extent.logical_start + i;
        *reserve_extent(write_stream) = fragment;
        memcpy(cluster_start(fragment.physical_start), cluster_start(input_extent.physical_start + i), fragment.length * meta_info.cluster_size);
        i += fragment.length;
    }
}

void fragment_extent(const extent& input_extent, StreamArchiver* write_stream) {
    printf("input_extent: %d %d %d\n", input_extent.physical_start, input_extent.length, input_extent.logical_start);

    uint32_t input_physical_end = input_extent.physical_start + input_extent.length,
             fragment_physical_start = input_extent.physical_start,
             i = find_blocked_extents(input_extent.physical_start);
    while(true) {
        if(i >= allocator.blocked_extent_count)
            break;
        extent* blocked_extent = &allocator.blocked_extents[i];
        if(input_physical_end < blocked_extent->physical_start)
            break;

        uint32_t blocked_physical_end = blocked_extent->physical_start + blocked_extent->length,
                 fragment_physical_end;
        bool is_blocked = blocked_extent->physical_start <= fragment_physical_start;
        if(is_blocked) {
            if(input_physical_end < blocked_physical_end)
                fragment_physical_end = input_physical_end;
            else
                fragment_physical_end = blocked_physical_end;
            ++i;
        } else
            fragment_physical_end = blocked_extent->physical_start;

        extent fragment;
        fragment.physical_start = fragment_physical_start;
        fragment.length = fragment.physical_start - fragment_physical_end;
        fragment.logical_start = input_extent.logical_start + (input_extent.physical_start - fragment.physical_start);
        fragment_physical_start = fragment_physical_end;

        printf("\tfragment: %d %d %d %d\n", fragment.physical_start, fragment.length, fragment.logical_start, is_blocked);

        if(is_blocked)
            resettle_extent(fragment, write_stream);
        else
            *reserve_extent(write_stream) = fragment;
    }
}

void aggregate_extents(uint32_t cluster_no, StreamArchiver* write_stream) {
    extent current_extent {0, 1, cluster_no};
    while(true) {
        bool is_end = cluster_no >= FAT_END_OF_CHAIN,
             is_consecutive = cluster_no == current_extent.physical_start + current_extent.length;
        if(is_end || !is_consecutive) {
            fragment_extent(current_extent, write_stream);
            current_extent.logical_start += current_extent.length;
            current_extent.length = 1;
            current_extent.physical_start = cluster_no;
        } else
            ++current_extent.length;
        if(is_end)
            break;
        cluster_no = *fat_entry(cluster_no);
    }
}

fat_dentry* read_lfn(fat_dentry* first_entry, StreamArchiver* extent_stream, uint16_t* name[], int lfn_entry_count, struct cluster_read_state* state) {
    uint8_t* entry = reinterpret_cast<uint8_t*>(first_entry);
    for (int i = lfn_entry_count - 1; i >= 0; i--) {
        lfn_cpy(name[i], entry);
        entry = reinterpret_cast<uint8_t*>(next_dentry(extent_stream, state));
    }
    return reinterpret_cast<fat_dentry*>(entry);
}

void traverse(StreamArchiver* dir_extent_stream, StreamArchiver* write_stream) {
    cluster_read_state state = {};

    uint32_t* children_count = reserve_children_count(write_stream);
    fat_dentry* current_dentry = init_cluster_read_state(dir_extent_stream, &state);

    while (!is_dir_table_end(current_dentry)) {
        bool has_long_name = is_lfn(current_dentry);
        if (has_long_name) {
            int lfn_entry_count = lfn_entry_sequence_no(current_dentry);
            uint16_t* name[lfn_entry_count];
            reserve_name(name, lfn_entry_count, write_stream);
            current_dentry = read_lfn(current_dentry, dir_extent_stream, name, lfn_entry_count, &state);
        } else {
            uint16_t* name[1];
            reserve_name(name, 1, write_stream);
            read_short_name(current_dentry, name[0]);
        }

        fat_dentry* dentry = reserve_dentry(write_stream);
        memcpy(dentry, current_dentry, sizeof *current_dentry);

        uint32_t cluster_no = file_cluster_no(current_dentry);
        StreamArchiver read_extent_stream = *write_stream;
        aggregate_extents(cluster_no, write_stream);
        if (is_dir(current_dentry)) {
            traverse(&read_extent_stream, write_stream);
        } else {
            *reserve_children_count(write_stream) = -1;
        }

        (*children_count)++;
        current_dentry = next_dentry(dir_extent_stream, &state);
    }
}

void init_stream_archiver(StreamArchiver* stream) {
    pageSize = 32;
    memset(stream, 0, sizeof *stream);
    cutStreamArchiver(stream);
}

int main(int argc, const char** argv) {
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
    init_extent_allocator();

    StreamArchiver stream;
    init_stream_archiver(&stream);
    StreamArchiver ext_stream = stream;
    aggregate_extents(boot_sector.root_cluster_no, &stream);
    traverse(&ext_stream, &stream);
}
