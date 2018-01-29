#include "fat.h"
#include "ext4.h"
#include "ext4_dentry.h"
#include "ext4_extent.h"
#include "ext4_inode.h"
#include "extent-allocator.h"
#include "stream-archiver.h"
#include "tree_builder.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

void build_ext4_root() {
    build_root_inode();
}

void skip_dir_extents(StreamArchiver *read_stream) {
    while (iterateStreamArchiver(read_stream, false, sizeof(fat_extent))) ;
}

ext4_dentry *build_dot_dirs(uint32_t dir_inode_no, uint32_t parent_inode_no, uint8_t *dot_dentry_p) {
    ext4_dentry dot_dentry = build_dot_dir_dentry(dir_inode_no);
    memcpy(dot_dentry_p, &dot_dentry, dot_dentry.rec_len);
    uint8_t *dot_dot_dentry_p = dot_dentry_p + dot_dentry.rec_len;

    ext4_dentry dot_dot_dentry = build_dot_dot_dir_dentry(parent_inode_no);
    memcpy(dot_dot_dentry_p, &dot_dot_dentry, dot_dot_dentry.rec_len);
    return (ext4_dentry *) dot_dot_dentry_p;
}

void build_lost_found() {
    fat_extent root_dentry_extent = allocate_extent(1);
    ext4_extent last_root_extent = last_extent(EXT4_ROOT_INODE);
    root_dentry_extent.logical_start = last_root_extent.ee_block + last_root_extent.ee_len;
    add_extent(&root_dentry_extent, EXT4_ROOT_INODE);

    build_lost_found_inode();
    ext4_dentry *dentry_address = (ext4_dentry *) cluster_start(root_dentry_extent.physical_start);
    ext4_dentry lost_found_dentry = build_lost_found_dentry();
    lost_found_dentry.rec_len = block_size();
    *dentry_address = lost_found_dentry;
    set_size(EXT4_ROOT_INODE, get_size(EXT4_ROOT_INODE) + block_size());

    // Build . and .. dirs in lost+found
    fat_extent lost_found_dentry_extent = allocate_extent(1);
    lost_found_dentry_extent.logical_start = 0;
    uint8_t *lost_found_dentry_p = cluster_start(lost_found_dentry_extent.physical_start);
    ext4_dentry *dot_dot_dentry = build_dot_dirs(EXT4_LOST_FOUND_INODE, EXT4_ROOT_INODE, lost_found_dentry_p);
    dot_dot_dentry->rec_len = block_size() - EXT4_DOT_DENTRY_SIZE;
    add_extent(&lost_found_dentry_extent, EXT4_LOST_FOUND_INODE);
    set_size(EXT4_LOST_FOUND_INODE, block_size());
}

void build_ext4_metadata_tree(uint32_t dir_inode_no, uint32_t parent_inode_no, StreamArchiver *read_stream) {
    skip_dir_extents(read_stream);
    uint32_t child_count = *(uint32_t*) iterateStreamArchiver(read_stream, false,
                                                              sizeof child_count);
    iterateStreamArchiver(read_stream, false, sizeof child_count);
    fat_extent dentry_extent = allocate_extent(1);
    dentry_extent.logical_start = 0;

    uint32_t block_count = 1;

    uint8_t *dentry_block_start = cluster_start(dentry_extent.physical_start);
    ext4_dentry *previous_dentry = build_dot_dirs(dir_inode_no, parent_inode_no, dentry_block_start);
    int position_in_block = 2 * EXT4_DOT_DENTRY_SIZE;

    for (uint32_t i = 0; i < child_count; i++) {
        fat_dentry *f_dentry = (fat_dentry *) iterateStreamArchiver(read_stream, false,
                                                                    sizeof *f_dentry);
        iterateStreamArchiver(read_stream, false, sizeof *f_dentry);
        uint32_t inode_number = build_inode(f_dentry);
        ext4_dentry *e_dentry = build_dentry(inode_number, read_stream);
        if (e_dentry->rec_len > block_size() - position_in_block) {
            previous_dentry->rec_len += block_size() - position_in_block;

            add_extent(&dentry_extent, dir_inode_no);

            dentry_extent = allocate_extent(1);
            dentry_extent.logical_start = block_count++;
            dentry_block_start = cluster_start(dentry_extent.physical_start);
        }
        previous_dentry = (ext4_dentry *) (dentry_block_start + position_in_block);
        position_in_block += e_dentry->rec_len;

        memcpy(previous_dentry, e_dentry, e_dentry->rec_len);
        free(e_dentry);

        if (!is_dir(f_dentry)) {
            set_extents(inode_number, f_dentry, read_stream);
        } else {
            build_ext4_metadata_tree(inode_number, dir_inode_no, read_stream);
        }
    }

    if (previous_dentry) {
        previous_dentry->rec_len += block_size() - position_in_block;
    }

    add_extent(&dentry_extent, dir_inode_no);
    set_size(dir_inode_no, block_count * block_size());
}
