#ifndef OFS_CONVERT_EXT4_BG_H
#define OFS_CONVERT_EXT4_BG_H

#include <stdint.h>
#include "ext4.h"

struct ext4_group_desc {
    uint32_t bg_block_bitmap_lo; /* Blocks bitmap block */
    uint32_t bg_inode_bitmap_lo; /* Inodes bitmap block */
    uint32_t bg_inode_table_lo; /* Inodes table block */
    uint16_t bg_free_blocks_count_lo;/* Free blocks count */
    uint16_t bg_free_inodes_count_lo;/* Free inodes count */
    uint16_t bg_used_dirs_count_lo; /* Directories count */
    uint16_t bg_flags;  /* EXT4_BG_flags (INODE_UNINIT, etc) */
    uint32_t bg_exclude_bitmap_lo;   /* Exclude bitmap for snapshots */
    uint16_t bg_block_bitmap_csum_lo;/* crc32c(s_uuid+grp_num+bbitmap) LE */
    uint16_t bg_inode_bitmap_csum_lo;/* crc32c(s_uuid+grp_num+ibitmap) LE */
    uint16_t bg_itable_unused_lo; /* Unused inodes count */
    uint16_t bg_checksum;  /* crc16(sb_uuid+group+desc) */
    uint32_t bg_block_bitmap_hi; /* Blocks bitmap block MSB */
    uint32_t bg_inode_bitmap_hi; /* Inodes bitmap block MSB */
    uint32_t bg_inode_table_hi; /* Inodes table block MSB */
    uint16_t bg_free_blocks_count_hi;/* Free blocks count MSB */
    uint16_t bg_free_inodes_count_hi;/* Free inodes count MSB */
    uint16_t bg_used_dirs_count_hi; /* Directories count MSB */
    uint16_t bg_itable_unused_hi;    /* Unused inodes count MSB */
    uint32_t bg_exclude_bitmap_hi;   /* Exclude bitmap block MSB */
    uint16_t bg_block_bitmap_csum_hi;/* crc32c(s_uuid+grp_num+bbitmap) BE */
    uint16_t bg_inode_bitmap_csum_hi;/* crc32c(s_uuid+grp_num+ibitmap) BE */
    uint32_t bg_reserved;
};

uint32_t block_group_count(const ext4_super_block& sb);

uint32_t block_group_start(const ext4_super_block& sb, uint32_t num);

uint32_t block_group_blocks(const ext4_super_block& sb);

uint32_t block_group_overhead(const ext4_super_block& sb);

void block_group_meta_extents(const ext4_super_block& sb, extent *list_out);

#endif //OFS_CONVERT_EXT4_BG_H
