#include "ext4.h"

#include <stdio.h>
#include <stdlib.h>


uint32_t log2(uint32_t value) {
    return sizeof(value) * 8 - __builtin_clz(value);
}


uint32_t min(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}


uint64_t from_lo_hi(uint32_t lo, uint32_t hi) {
    return static_cast<uint64_t>(hi) << 32 | lo;
}


void set_lo_hi(uint32_t& lo, uint32_t& hi, uint64_t value) {
    lo = static_cast<uint32_t>(value & 0xFFFFFFFF);
    hi = static_cast<uint32_t>(value >> 32);
}


template <class T>
uint32_t ceildiv(T a, T b) {
    return (a + b - 1) / b;
}


uint32_t block_size(const ext4_super_block& sb) {
    return 1u << (sb.s_log_block_size + EXT4_BLOCK_SIZE_MIN_LOG2);
}


uint8_t *block_start(uint32_t block_no, ext4_super_block& sb) {
    return meta_info.data_start + block_no * block_size(sb);
}


uint16_t calc_reserved_gdt_blocks(const ext4_super_block& sb) {
    // This logic is copied from the one in the official mke2fs (http://e2fsprogs.sourceforge.net/)
    uint64_t block_count = from_lo_hi(sb.s_blocks_count_lo, sb.s_blocks_count_hi);
    uint32_t max_blocks = block_count > (0xFFFFFFFF / 1024)
                          ? 0xFFFFFFFF
                          : static_cast<uint32_t>(block_count * 1024);
    uint32_t group_count = ceildiv(max_blocks, sb.s_blocks_per_group);
    uint32_t gdt_block_count = ceildiv(group_count * sb.s_desc_size, block_size(sb));

    // No idea why this limit is needed
    return static_cast<uint16_t>(min(gdt_block_count,
                                     block_size(sb) / sizeof(uint32_t)));
}


uint32_t block_group_overhead(const ext4_super_block& sb) {
    uint32_t inode_table_blocks = ceildiv(sb.s_inodes_per_group * sb.s_inode_size,
                                          block_size(sb));
    return 3 + sb.s_reserved_gdt_blocks + inode_table_blocks;
}


uint32_t block_group_count(const ext4_super_block& sb) {
    uint64_t block_count = from_lo_hi(sb.s_blocks_count_lo, sb.s_blocks_count_hi);
    return static_cast<uint32_t>(ceildiv<uint64_t>(block_count, sb.s_blocks_per_group));
}

ext4_super_block create_ext4_sb() {
    uint32_t bytes_per_block = boot_sector.bytes_per_sector * boot_sector.sectors_per_cluster;
    uint64_t partition_bytes = boot_sector.bytes_per_sector * static_cast<uint64_t>(sector_count());

    if (bytes_per_block < 1024) {
        fprintf(stderr, "This tool only works for FAT partitions with cluster size >= 1kB\n");
        exit(1);
    }

    // TODO: Set all (relevant) values
    ext4_super_block sb{};

    sb.s_magic = EXT4_MAGIC;
    sb.s_state = EXT4_STATE_CLEANLY_UNMOUNTED;
    sb.s_feature_incompat = EXT4_FEATURE_INCOMPAT_64BIT | EXT4_FEATURE_INCOMPAT_EXTENTS;
    sb.s_feature_compat = EXT4_FEATURE_COMPAT_RESIZE_INODE;
    sb.s_desc_size = EXT4_64BIT_DESC_SIZE;
    sb.s_inode_size = EXT4_INODE_SIZE;
    sb.s_rev_level = EXT4_DYNAMIC_REV;

    sb.s_log_block_size = log2(bytes_per_block) - EXT4_BLOCK_SIZE_MIN_LOG2;
    sb.s_first_data_block = bytes_per_block == 1024 ? 1 : 0;
    sb.s_blocks_per_group = bytes_per_block * 8;
    uint64_t block_count = partition_bytes / bytes_per_block;
    set_lo_hi(sb.s_blocks_count_lo, sb.s_blocks_count_hi, block_count);
    sb.s_reserved_gdt_blocks = calc_reserved_gdt_blocks(sb);

    uint32_t bg_count = block_group_count(sb);
    // Same logic as used in mke2fs: If the last block group would support have
    // fewer than 50 data blocks, than reduce the block count and ignore the
    // remaining space
    // For some reason in tests we found that mkfs.ext4 didn't follow this logic
    // and instead set sb.blocks_per_group to a value lower than
    // bytes_per_block * 8, but this is easier to implement.
    if (block_count % sb.s_blocks_per_group < block_group_overhead(sb) + 50) {
        set_lo_hi(sb.s_blocks_count_lo, sb.s_blocks_count_hi, block_count);
        sb.s_reserved_gdt_blocks = calc_reserved_gdt_blocks(sb);
    }

    sb.s_inodes_per_group = min(
            // This is the same logic as used by mke2fs to determine the inode count
            sb.s_blocks_per_group * bytes_per_block / EXT4_INODE_RATIO,
            // Inodes per group need to fit into a one page bitmap
            bytes_per_block * 8);
    sb.s_inodes_count = sb.s_inodes_per_group * bg_count;

    return sb;
}

void block_group_meta_extents(const ext4_super_block& sb, extent *list_out) {
    uint32_t bg_size = sb.s_blocks_per_group;
    uint32_t bg_overhead = block_group_overhead(sb);
    for (uint32_t i = 0; i < block_group_count(sb); ++i) {
        *list_out++ = { 0, bg_overhead, block_group_start(sb, i) };
    }
}
