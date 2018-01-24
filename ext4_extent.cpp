#include "extent-allocator.h"
#include "ext4.h"
#include "ext4_bg.h"
#include "ext4_extent.h"
#include "ext4_inode.h"
#include "fat.h"
#include "stream-archiver.h"
#include "util.h"

#include <string.h>

ext4_extent_header init_extent_header() {
    ext4_extent_header header;
    header.eh_entries = 0;
    header.eh_max = 4;
    header.eh_depth = 0;
    header.eh_generation = 0;
    return header;
}

ext4_extent to_ext4_extent(extent *e) {
    return {
        // what if fat extent longer than uint16?
        .ee_block = e->logical_start,
        .ee_len = e->length,
        .ee_start_hi = 0,
        .ee_start_lo = e->physical_start
    };
}

void append_to_new_idx_path(uint16_t depth, extent *ext, ext4_extent_idx *idx) {
    for (int i = depth; i > depth; i--) {
        uint32_t block_no = allocate_extent(1).physical_start;
        *idx = {
            .ei_block = ext->logical_start,
            .ei_leaf_lo = block_no,
            .ei_leaf_hi = 0
        };

        ext4_extent_header *header = (ext4_extent_header *) block_start(idx->ei_leaf_lo);
        header->eh_entries = 1;
        header->eh_max = block_size() / sizeof(ext4_extent);
        header->eh_depth = i;

        idx = (ext4_extent_idx *) (header + 1);
    }
    ext4_extent *actual_extent = (ext4_extent *) idx;
    *actual_extent = to_ext4_extent(ext);
}

bool append_in_block(ext4_extent_header *header, extent *ext) {
    if (header->eh_entries >= header->eh_max) return false;

    ext4_extent *new_entry = (ext4_extent *) (header + header->eh_entries + 1);
    // move tail
    memcpy(new_entry, new_entry + 1, sizeof(ext4_extent_tail));
    // TODO update tail

    *new_entry = to_ext4_extent(ext);
    header->eh_entries++;
    return true;
}

bool append_to_extent_tree(extent *ext, ext4_extent_header *root_header) {
    if (root_header->eh_depth == 0) {
        bool success = append_in_block(root_header, ext);
        return success;
    }

    uint16_t entry_count = root_header->eh_entries;
    ext4_extent *last_child_entry = (ext4_extent *) (root_header + entry_count);
    uint32_t child_block = last_child_entry->ee_block;
    ext4_extent_header *child_header = (ext4_extent_header *) block_start(child_block);
    if (append_to_extent_tree(ext, child_header)) {
        return true;
    }

    if (entry_count < root_header->eh_max) {
        ext4_extent_idx *idx = (ext4_extent_idx *) (root_header + entry_count);
        append_to_new_idx_path(root_header->eh_depth - 1, ext, idx);
        return true;
    } else {
        return false;
    }
}

void make_tree_deeper(ext4_extent_header *root_header) {
    uint32_t block_no  = allocate_extent(1).physical_start;
    uint8_t *child_block = block_start(block_no);
    memcpy(child_block, root_header, 5 * sizeof *root_header);  // copy header and all nodes from the inode

    ext4_extent_header *child_header = (ext4_extent_header *) child_block;
    child_header->eh_max = block_size() / sizeof(ext4_extent);

    ext4_extent_tail *child_tail = (ext4_extent_tail *) (child_header + 5);
    // TODO create tail
    root_header->eh_depth++;
    root_header->eh_entries = 1;
    ext4_extent_idx *idx = (ext4_extent_idx *) (root_header + 1);
    ext4_extent *child_first_extent = (ext4_extent *)(child_header + 1);  // could also be idx, irrelevant
    *idx = {
        .ei_block = child_first_extent->ee_block,
        .ei_leaf_lo = block_no,
        .ei_leaf_hi = 0
    };
}

void add_extent(extent *ext, uint32_t inode_number) {
    ext4_inode *inode = &get_existing_inode(inode_number);
    ext4_extent_header *header = &(inode->ext_header);
    bool success = append_to_extent_tree(ext, header);

    if (!success) {
        make_tree_deeper(header);
        // attempt adding extent again, should succeed this time
        append_to_extent_tree(ext, header);
    }

    inode->i_blocks_lo += ext->length;
}

void set_extents(uint32_t inode_number, fat_dentry *dentry, StreamArchiver *read_stream) {
    set_size(inode_number, dentry->file_size);
    extent *current_extent = (extent *) iterateStreamArchiver(read_stream, false, sizeof *current_extent);
    while (current_extent != NULL) {
        add_extent(current_extent, inode_number);
        current_extent = (extent *) iterateStreamArchiver(read_stream, false, sizeof *current_extent);
    }
}
