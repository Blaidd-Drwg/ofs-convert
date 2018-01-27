#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ext4_bg.h"
#include "ext4_inode.h"
#include "util.h"


ext4_group_desc *group_descs;


uint32_t block_group_count() {
    uint64_t block_count = from_lo_hi(sb.s_blocks_count_lo, sb.s_blocks_count_hi);
    return static_cast<uint32_t>(ceildiv<uint64_t>(block_count, sb.s_blocks_per_group));
}


uint32_t gdt_block_count() {
    return ceildiv(block_group_count(), block_size() / sb.s_desc_size);
}


uint32_t block_group_block_count(uint32_t num) {
    uint64_t start = block_group_start(num);
    uint64_t blocks_total = from_lo_hi(sb.s_blocks_count_lo, sb.s_blocks_count_hi);
    return min(sb.s_blocks_per_group, static_cast<uint32_t>(blocks_total - start));
}


uint32_t inode_table_blocks() {
    return ceildiv(sb.s_inodes_per_group * sb.s_inode_size, block_size());
}


uint32_t block_group_overhead() {
    return 3 + gdt_block_count() + sb.s_reserved_gdt_blocks + inode_table_blocks();
}


uint64_t block_group_start(uint32_t num) {
    return sb.s_blocks_per_group * num + sb.s_first_data_block;
}


fat_extent *create_block_group_meta_extents() {
    uint32_t bg_count = block_group_count();
    auto * extents = static_cast<fat_extent *>(malloc(bg_count * sizeof(fat_extent)));
    uint32_t bg_overhead = block_group_overhead();
    if (bg_overhead > 0xFFFF) {
        fprintf(stderr, "Block group overhead too large\n");
        exit(1);
    }

    for (uint32_t i = 0; i < block_group_count(); ++i) {
        extents[i] = {0, static_cast<uint16_t>(bg_overhead),
                      static_cast<uint32_t>(block_group_start(i))};
    }

    return extents;
}


void init_ext4_group_descs() {
    uint32_t bg_count = block_group_count();
    uint32_t gdt_blocks = gdt_block_count();
    uint32_t bg_overhead = block_group_overhead();
    uint32_t blk_size = block_size();
    uint32_t itable_blocks = inode_table_blocks();

    group_descs = static_cast<ext4_group_desc *>(malloc(bg_count * sizeof(ext4_group_desc)));
    memset(group_descs, 0, bg_count * sizeof(ext4_group_desc));

    for (uint32_t i = 0; i < bg_count; ++i) {
        ext4_group_desc& bg = group_descs[i];
        uint64_t bg_start_block = block_group_start(i);
        uint32_t block_count = block_group_block_count(i);
        uint32_t used_inodes = i == 0 ? EXT4_FIRST_NON_RSV_INODE - 1 : 0;

        uint64_t block_bitmap_block = bg_start_block + 1 + gdt_blocks + sb.s_reserved_gdt_blocks;
        uint64_t inode_bitmap_block = bg_start_block + 2 + gdt_blocks + sb.s_reserved_gdt_blocks;
        uint64_t inode_table_block = bg_start_block + 3 + gdt_blocks + sb.s_reserved_gdt_blocks;
        set_lo_hi(bg.bg_block_bitmap_lo, bg.bg_block_bitmap_hi, block_bitmap_block);
        set_lo_hi(bg.bg_inode_bitmap_lo, bg.bg_inode_bitmap_hi, inode_bitmap_block);
        set_lo_hi(bg.bg_inode_table_lo, bg.bg_inode_table_hi, inode_table_block);
        set_lo_hi(bg.bg_free_inodes_count_lo, bg.bg_free_inodes_count_hi,
                  sb.s_inodes_per_group - used_inodes);
        set_lo_hi(bg.bg_free_blocks_count_lo, bg.bg_free_blocks_count_hi,
                  block_count - bg_overhead);

        uint8_t *block_bitmap = block_start(block_bitmap_block);
        uint8_t *inode_bitmap = block_start(inode_bitmap_block);
        uint8_t *inode_table = block_start(inode_table_block);

        memset(block_bitmap, 0, blk_size);
        bitmap_set_bits(block_bitmap, 0, bg_overhead);
        bitmap_set_bits(block_bitmap, block_count, sb.s_blocks_per_group);
        memset(inode_bitmap, 0, blk_size);
        bitmap_set_bits(inode_bitmap, 0, used_inodes);
        bitmap_set_bits(inode_bitmap, sb.s_inodes_per_group, blk_size * 8);
        memset(inode_table, 0, blk_size * itable_blocks);
    }
}


void add_inode(const ext4_inode& inode, uint32_t inode_num) {
    uint32_t bg_num = (inode_num - 1) / sb.s_inodes_per_group;
    uint32_t num_in_bg = (inode_num - 1) % sb.s_inodes_per_group;
    ext4_group_desc& bg = group_descs[bg_num];

    uint8_t *inode_bitmap = block_start(from_lo_hi(bg.bg_inode_bitmap_lo, bg.bg_inode_bitmap_hi));
    uint8_t *inode_table = block_start(from_lo_hi(bg.bg_inode_table_lo, bg.bg_inode_table_hi));

    bitmap_set_bit(inode_bitmap, num_in_bg);
    memcpy(inode_table + num_in_bg * sb.s_inode_size, &inode, sizeof(inode));

    decr_lo_hi(bg.bg_free_inodes_count_lo, bg.bg_free_inodes_count_hi);
    if (inode.i_mode & S_IFDIR) {
        incr_lo_hi(bg.bg_used_dirs_count_lo, bg.bg_used_dirs_count_hi);
    }
}


void add_reserved_inode(const ext4_inode& inode, uint32_t inode_num) {
    uint32_t bg_num = (inode_num - 1) / sb.s_inodes_per_group;
    uint32_t num_in_bg = (inode_num - 1) % sb.s_inodes_per_group;
    ext4_group_desc& bg = group_descs[bg_num];

    uint8_t *inode_table = block_start(from_lo_hi(bg.bg_inode_table_lo, bg.bg_inode_table_hi));
    memcpy(inode_table + num_in_bg * sb.s_inode_size, &inode, sizeof(inode));
    if (inode.i_mode & S_IFDIR) {
        incr_lo_hi(bg.bg_used_dirs_count_lo, bg.bg_used_dirs_count_hi);
    }
}


void add_extent_to_block_bitmap(uint64_t blocks_begin, uint64_t blocks_end) {
    // We assume the extent is correct, i.e. only inside a single block group
    auto bg_num = static_cast<uint32_t>((blocks_begin - sb.s_first_data_block) / sb.s_blocks_per_group);
    ext4_group_desc& bg = group_descs[bg_num];
    uint64_t bg_block_start = block_group_start(bg_num);
    uint8_t *block_bitmap = block_start(from_lo_hi(bg.bg_block_bitmap_lo, bg.bg_block_bitmap_hi));

    bitmap_set_bits(block_bitmap,
                    static_cast<uint32_t>(blocks_begin - bg_block_start),
                    static_cast<uint32_t>(blocks_end - bg_block_start));
    decr_lo_hi(bg.bg_free_blocks_count_lo, bg.bg_free_blocks_count_hi,
               static_cast<uint32_t>(blocks_end - blocks_begin));
}


ext4_inode& get_existing_inode(uint32_t inode_num) {
    uint32_t bg_num = (inode_num - 1) / sb.s_inodes_per_group;
    uint32_t num_in_bg = (inode_num - 1) % sb.s_inodes_per_group;
    ext4_group_desc& bg = group_descs[bg_num];
    uint8_t *inode_table = block_start(from_lo_hi(bg.bg_inode_table_lo, bg.bg_inode_table_hi));
    return *reinterpret_cast<ext4_inode*>(inode_table + num_in_bg * sb.s_inode_size);
}


void finalize_block_groups_on_disk() {
    uint32_t bg_count = block_group_count();
    for (uint16_t i = 0; i < bg_count; ++i) {
        ext4_group_desc& bg = group_descs[i];
        sb.s_free_inodes_count += from_lo_hi(bg.bg_free_inodes_count_lo,
                                             bg.bg_free_inodes_count_hi);
        incr_lo_hi(sb.s_free_blocks_count_lo, sb.s_free_blocks_count_hi,
                   from_lo_hi(bg.bg_free_blocks_count_lo, bg.bg_free_blocks_count_hi));
    }

    ext4_super_block sb_copy = sb;
    for (uint16_t i = 0; i < bg_count; ++i) {
        sb_copy.s_block_group_nr = i;
        uint64_t bg_block_start = block_group_start(i);
        uint32_t sb_offset = (i == 0 && block_size() != 1024) ? 1024 : 0;
        memcpy(block_start(bg_block_start) + sb_offset, &sb_copy,
               sizeof(ext4_super_block));
        memcpy(block_start(bg_block_start + 1), group_descs,
               bg_count * sizeof(ext4_group_desc));
    }
}
