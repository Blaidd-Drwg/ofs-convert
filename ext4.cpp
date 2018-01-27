#include "ext4.h"
#include "ext4_bg.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>


ext4_super_block sb;


uint32_t block_size() {
    return 1u << (sb.s_log_block_size + EXT4_BLOCK_SIZE_MIN_LOG2);
}


uint8_t *block_start(uint64_t block_no) {
    return meta_info.fs_start + block_no * block_size();
}


void init_ext4_sb() {
    uint32_t bytes_per_block = boot_sector.bytes_per_sector * boot_sector.sectors_per_cluster;
    uint64_t partition_bytes = boot_sector.bytes_per_sector * static_cast<uint64_t>(sector_count());

    if (bytes_per_block < 1024) {
        fprintf(stderr, "This tool only works for FAT partitions with cluster size >= 1kB\n");
        exit(1);
    }

    // TODO: Set all (relevant) values
    sb.s_magic = EXT4_MAGIC;
    sb.s_state = EXT4_STATE_CLEANLY_UNMOUNTED;
    sb.s_feature_incompat = EXT4_FEATURE_INCOMPAT_64BIT | EXT4_FEATURE_INCOMPAT_EXTENTS;
    sb.s_desc_size = EXT4_64BIT_DESC_SIZE;
    sb.s_inode_size = EXT4_INODE_SIZE;
    sb.s_rev_level = EXT4_DYNAMIC_REV;
    sb.s_errors = EXT4_ERRORS_DEFAULT;
    sb.s_first_ino = EXT4_FIRST_NON_RSV_INODE;

    sb.s_log_block_size = log2(bytes_per_block) - EXT4_BLOCK_SIZE_MIN_LOG2;
    sb.s_first_data_block = bytes_per_block == 1024 ? 1 : 0;
    sb.s_blocks_per_group = bytes_per_block * 8;
    uint64_t block_count = partition_bytes / bytes_per_block;
    set_lo_hi(sb.s_blocks_count_lo, sb.s_blocks_count_hi, block_count);

    // These have to have these values even if bigalloc is disabled
    sb.s_log_cluster_size = sb.s_log_block_size;
    sb.s_clusters_per_group = sb.s_blocks_per_group;

    // Same logic as used in mke2fs: If the last block group would support have
    // fewer than 50 data blocks, than reduce the block count and ignore the
    // remaining space
    // For some reason in tests we found that mkfs.ext4 didn't follow this logic
    // and instead set sb.blocks_per_group to a value lower than
    // bytes_per_block * 8, but this is easier to implement.
    if (block_count % sb.s_blocks_per_group < block_group_overhead() + 50) {
        set_lo_hi(sb.s_blocks_count_lo, sb.s_blocks_count_hi, block_count);
    }

    sb.s_inodes_per_group = min(
            // This is the same logic as used by mke2fs to determine the inode count
            sb.s_blocks_per_group * bytes_per_block / EXT4_INODE_RATIO,
            // Inodes per group need to fit into a one page bitmap
            bytes_per_block * 8);
    sb.s_inodes_count = sb.s_inodes_per_group * block_group_count();
}
