#include "fat.h"
#include "ext4.h"
#include "ext4_bg.h"
#include "ext4_extent.h"
#include "extent-allocator.h"
#include "stream-archiver.h"
#include "ext4_inode.h"
#include "util.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

uint32_t first_free_inode_no = 0;

uint32_t save_inode(ext4_inode *inode) {
    add_inode(*inode, first_free_inode_no);
    return first_free_inode_no++;
}

uint32_t build_inode(fat_dentry *dentry) {
    ext4_inode inode;
    memset(&inode, 0, sizeof inode);
    inode.i_mode = 0733 | (is_dir(dentry) ? 0400000 : 0100000);
    inode.i_uid = geteuid() & 0xFFFF;
    inode.l_i_uid_high = geteuid() >> 16;
    inode.i_gid = getegid() & 0xFFFF;
    inode.l_i_gid_high = getegid() >> 16;
    inode.i_atime = fat_time_to_unix(dentry->access_date, 0);
    inode.i_ctime = fat_time_to_unix(dentry->create_date, dentry->create_time);
    inode.i_mtime = fat_time_to_unix(dentry->mod_date, dentry->mod_time);
    inode.i_links_count = 1; // TODO fuck hardlinks
    inode.i_flags = 0x80000;  // uses extents
    inode.ext_header = init_extent_header();

    // TODO checksum
    return save_inode(&inode);
}

void set_size(uint32_t inode_no, uint64_t size) {
    ext4_inode& inode = get_existing_inode(inode_no);
    set_lo_hi(inode.i_size_lo, inode.i_size_high, size);
}
