#include "fat.h"
#include "ext4.h"
#include "ext4_extent.h"
#include "extent-allocator.h"
#include "stream-archiver.h"
#include "ext4_inode.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int first_free_inode_no = 0;

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

void set_size(int inode_no, uint64_t size) {
    ext4_inode *inode = get_inode(inode_no);
    inode->i_size_lo = size & 0xFFFFFFFF;
    inode->i_size_high = size >> 32;
}

int save_inode(ext4_inode *inode) {
    ext4_inode *free_inode = get_inode(first_free_inode_no);
    memcpy(free_inode, inode, sizeof *inode);
    return first_free_inode_no++;
}

struct ext4_inode *get_inode(uint32_t inode_no, ext4_super_block *sb) {
    int bg_no = inode_no / sb->s_inodes_per_group;
    ext4_inode *inode_table_start = NULL;  // TODO
    return inode_table_start + (inode_no % sb->s_inodes_per_group);
}

