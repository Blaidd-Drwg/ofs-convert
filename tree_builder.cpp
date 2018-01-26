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

void build_ext4_metadata_tree(uint32_t parent_inode_number, StreamArchiver *read_stream) {
    uint32_t child_count = *(uint32_t*) iterateStreamArchiver(read_stream, false,
                                                              sizeof child_count);
    fat_extent dentry_extent = allocate_extent(1);
    dentry_extent.logical_start = 0;

    int position_in_block = 0;
    ext4_dentry *previous_dentry;
    uint32_t block_count = 1;

    for (uint32_t i = 0; i < child_count; i++) {
        fat_dentry *f_dentry = (fat_dentry *) iterateStreamArchiver(read_stream, false,
                                                                    sizeof *f_dentry);
        uint32_t inode_number = build_inode(f_dentry);
        ext4_dentry *e_dentry = build_dentry(inode_number, read_stream);
        if (e_dentry->rec_len > block_size() - position_in_block) {
            previous_dentry->rec_len += block_size() - position_in_block;

            add_extent(&dentry_extent, parent_inode_number);

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
            build_ext4_metadata_tree(inode_number, read_stream);
        }
     }
    set_size(parent_inode_number, block_count * block_size());
}
