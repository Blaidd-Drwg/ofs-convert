#include "ext4_bg.h"
#include "util.h"


uint32_t block_group_count(const ext4_super_block& sb) {
    uint64_t block_count = from_lo_hi(sb.s_blocks_count_lo, sb.s_blocks_count_hi);
    return static_cast<uint32_t>(ceildiv<uint64_t>(block_count, sb.s_blocks_per_group));
}


uint32_t block_group_blocks(const ext4_super_block& sb) {
    return ceildiv(block_group_count(sb), block_size(sb) / sb.s_desc_size);
}


uint32_t block_group_overhead(const ext4_super_block& sb) {
    uint32_t inode_table_blocks = ceildiv(sb.s_inodes_per_group,
                                          block_size(sb) / sb.s_inode_size);
    return 3 + block_group_blocks(sb) + sb.s_reserved_gdt_blocks + inode_table_blocks;
}


uint32_t block_group_start(const ext4_super_block& sb, uint32_t num) {
    return sb.s_blocks_per_group * num + sb.s_first_data_block;
}


void block_group_meta_extents(const ext4_super_block& sb, extent *list_out) {
    uint32_t bg_overhead = block_group_overhead(sb);
    for (uint32_t i = 0; i < block_group_count(sb); ++i) {
        *list_out++ = { 0, bg_overhead, block_group_start(sb, i) };
    }
}


ext4_group_desc create_basic_group_desc(const ext4_super_block& sb) {
    ext4_group_desc bg{};
    uint32_t gdt_blocks = block_group_blocks(sb);
    set_lo_hi(bg.bg_block_bitmap_lo, bg.bg_block_bitmap_hi,
              1 + gdt_blocks + sb.s_reserved_gdt_blocks);
    set_lo_hi(bg.bg_inode_bitmap_lo, bg.bg_inode_bitmap_hi,
              2 + gdt_blocks + sb.s_reserved_gdt_blocks);
    set_lo_hi(bg.bg_inode_table_lo, bg.bg_inode_table_hi,
              3 + gdt_blocks + sb.s_reserved_gdt_blocks);
    set_lo_hi(bg.bg_free_inodes_count_lo, bg.bg_free_inodes_count_hi,
              sb.s_inodes_per_group);

    // Checksums are calculated after inode and directory counts are finalized
    return bg;
}
