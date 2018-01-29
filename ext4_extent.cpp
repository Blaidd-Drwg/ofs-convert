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

ext4_extent to_ext4_extent(fat_extent *fext) {
    ext4_extent eext;
    eext.ee_block = fext->logical_start;
    eext.ee_len = fext->length;
    set_lo_hi(eext.ee_start_lo, eext.ee_start_hi, fat_sector_to_ext4_block(fext->physical_start));
    return eext;
}

void append_to_new_idx_path(uint16_t depth, ext4_extent *ext, ext4_extent_idx *idx) {
    for (int i = depth; i > depth; i--) {
        uint32_t block_no = fat_sector_to_ext4_block(allocate_extent(1).physical_start);
        *idx = {
            .ei_block = ext->ee_block,
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
    memcpy(actual_extent, ext, sizeof *ext);
}

bool append_in_block(ext4_extent_header *header, ext4_extent *ext) {
    if (header->eh_entries >= header->eh_max) return false;

    ext4_extent *new_entry = (ext4_extent *) (header + header->eh_entries + 1);
    // move tail
    memcpy(new_entry + 1, new_entry, sizeof(ext4_extent_tail));
    // TODO update tail

    memcpy(new_entry, ext, sizeof *ext);
    header->eh_entries++;
    return true;
}

bool append_to_extent_tree(ext4_extent *ext, ext4_extent_header *root_header) {
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
    uint32_t block_no = fat_sector_to_ext4_block(allocate_extent(1).physical_start);
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

void add_extent(fat_extent *fext, uint32_t inode_number) {
    ext4_extent eext = to_ext4_extent(fext);
    ext4_inode *inode = &get_existing_inode(inode_number);
    ext4_extent_header *header = &(inode->ext_header);
    bool success = append_to_extent_tree(&eext, header);

    if (!success) {
        make_tree_deeper(header);
        // attempt adding extent again, should succeed this time
        append_to_extent_tree(&eext, header);
    }

    uint64_t extent_start_block = from_lo_hi(eext.ee_start_lo, eext.ee_start_hi);
    add_extent_to_block_bitmap(extent_start_block, extent_start_block + eext.ee_len);
    inode->i_blocks_lo += eext.ee_len * block_size() / 512;
}

void set_extents(uint32_t inode_number, fat_dentry *dentry, StreamArchiver *read_stream) {
    fat_extent *current_extent = (fat_extent *) iterateStreamArchiver(read_stream, false, sizeof *current_extent);
    while (current_extent != NULL) {
        add_extent(current_extent, inode_number);
        current_extent = (fat_extent *) iterateStreamArchiver(read_stream, false, sizeof *current_extent);
    }
    set_size(inode_number);
}

ext4_extent last_extent(uint32_t inode_number) {
    ext4_inode *inode = &get_existing_inode(inode_number);
    ext4_extent_header *header = &(inode->ext_header);

    while(header->eh_depth) {
        ext4_extent_idx *last_idx = (ext4_extent_idx *) (header + header->eh_entries);
        header = (ext4_extent_header *) block_start(from_lo_hi(last_idx->ei_leaf_lo, last_idx->ei_leaf_hi));
    }

    return *(ext4_extent *) (header + header->eh_entries);
}
