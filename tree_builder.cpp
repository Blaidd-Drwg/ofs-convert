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

void build_lost_found() {
    ext4_extent last_root_extent = last_extent(EXT4_ROOT_INODE);
    uint32_t logical_start = last_root_extent.ee_block + last_root_extent.ee_len;

    fat_extent dentry_extent = allocate_extent(1);
    dentry_extent.logical_start = logical_start;
    add_extent(&dentry_extent, EXT4_ROOT_INODE);

    build_lost_found_inode();
    ext4_dentry *dentry_address = (ext4_dentry *) block_start(fat_sector_to_ext4_block(dentry_extent.physical_start));
    ext4_dentry dentry = build_lost_found_dentry();
    dentry.rec_len = block_size();
    *dentry_address = dentry;

    set_size(EXT4_ROOT_INODE, get_size(EXT4_ROOT_INODE) + block_size());
}


uint16_t build_dot_dirs(uint32_t dir_inode_no, uint32_t parent_inode_no, uint8_t *dentry_block) {
    ext4_dentry dot_dentry = build_dot_dir_dentry(dir_inode_no);
    memcpy(dentry_block, &dot_dentry, dot_dentry.rec_len);
    uint16_t blub = dot_dentry.rec_len;

    ext4_dentry dot_dot_dentry = build_dot_dot_dir_dentry(parent_inode_no);
    memcpy(dentry_block + blub, &dot_dot_dentry, dot_dot_dentry.rec_len);
    return blub + dot_dentry.rec_len;
}

void build_ext4_metadata_tree(uint32_t dir_inode_no, uint32_t parent_inode_no, StreamArchiver *read_stream) {
    uint32_t child_count = *(uint32_t*) iterateStreamArchiver(read_stream, false,
                                                              sizeof child_count);
    fat_extent dentry_extent = allocate_extent(1);
    dentry_extent.logical_start = 0;

    ext4_dentry *previous_dentry;
    uint32_t block_count = 1;

    uint8_t *dentry_block = cluster_start(dentry_extent.physical_start);
    int position_in_block = build_dot_dirs(dir_inode_no, parent_inode_no, dentry_block);

    for (uint32_t i = 0; i < child_count; i++) {
        fat_dentry *f_dentry = (fat_dentry *) iterateStreamArchiver(read_stream, false,
                                                                    sizeof *f_dentry);
        uint32_t inode_number = build_inode(f_dentry);
        ext4_dentry *e_dentry = build_dentry(inode_number, read_stream);
        if (e_dentry->rec_len > block_size() - position_in_block) {
            previous_dentry->rec_len += block_size() - position_in_block;

            add_extent(&dentry_extent, dir_inode_no);

            dentry_extent = allocate_extent(1);
            dentry_extent.logical_start = block_count++;
        }
        ext4_dentry *dentry_address = (ext4_dentry *) block_start(fat_sector_to_ext4_block(dentry_extent.physical_start)) + position_in_block;
        memcpy(dentry_address, e_dentry, e_dentry->rec_len);
        position_in_block += e_dentry->rec_len;
        previous_dentry = dentry_address;
        free(e_dentry);

        if (!is_dir(f_dentry)) {
            set_extents(inode_number, f_dentry, read_stream);
        } else {
            while (iterateStreamArchiver(read_stream, false, sizeof(fat_extent))) ;  // consume extents
            build_ext4_metadata_tree(inode_number, dir_inode_no, read_stream);
        }
    }
    add_extent(&dentry_extent, dir_inode_no);
    set_size(dir_inode_no, block_count * block_size());
}
