#include "fat.h"
#include "ext4.h"
#include "ext4_dentry.h"
#include "ext4_extent.h"
#include "ext4_inode.h"
#include "extent-allocator.h"
#include "stream-archiver.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

// TODO:
// ez:
// remove unnecessary includes
//
// mid:
// get inode table start
//
// hard:
// checksum
// handle hardlinks
// merge dir extents

void build_ext4_metadata_tree(uint32_t parent_inode_number, StreamArchiver *read_stream, ext4_super_block *sb) {
    uint32_t child_count = *(uint32_t*) iterateStreamArchiver(read_stream, false,
                                                              sizeof child_count);
    extent dentry_extent = allocate_extent(1);
    dentry_extent.logical_start = 0;

    int position_in_block = 0;
    ext4_dentry *previous_dentry;
    int block_count = 1;

    for (uint32_t i = 0; i < child_count; i++) {
        fat_dentry *f_dentry = (fat_dentry *) iterateStreamArchiver(read_stream, false,
                                                                    sizeof *f_dentry);
        uint32_t inode_number = build_inode(f_dentry);
        ext4_dentry *e_dentry = build_dentry(inode_number, read_stream);
        if (e_dentry->rec_len > block_size(*sb) - position_in_block) {
            previous_dentry->rec_len += block_size(*sb) - position_in_block;

            add_extent(&dentry_extent, parent_inode_number, sb);

            dentry_extent = allocate_extent(1);
            dentry_extent.logical_start = block_count++;
        }
        ext4_dentry *dentry_address = (ext4_dentry *) block_start(dentry_extent.physical_start, *sb) + position_in_block;
        memcpy(dentry_address, e_dentry, e_dentry->rec_len);
        position_in_block += e_dentry->rec_len;
        previous_dentry = dentry_address;
        free(e_dentry);

        if (!is_dir(f_dentry)) {
            set_extents(inode_number, f_dentry, read_stream, sb);
        } else {
            while (iterateStreamArchiver(read_stream, false, sizeof(extent))) ;  // consume extents
            build_ext4_metadata_tree(inode_number, read_stream, sb);
        }
     }
    set_size(parent_inode_number, block_count * block_size(*sb));
}
