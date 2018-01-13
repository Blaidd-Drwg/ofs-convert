#include "block-allocate.h"

#include <stdlib.h>

uint32_t max(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

bool is_free_cluster(uint32_t cluster_entry) {
    return (cluster_entry & CLUSTER_ENTRY_MASK) == FREE_CLUSTER;
}

// Sort cluster extents by ascending start indices
int compare_extents(const void *e1, const void *e2) {
    const auto *left = reinterpret_cast<const cluster_extent *>(e1);
    const auto *right = reinterpret_cast<const cluster_extent *>(e2);
    return left->start_index - right->start_index;
}

block_allocator make_block_allocator(cluster_extent *blocked_extents,
                                     cluster_extent *blocked_extents_end) {
    qsort(blocked_extents, blocked_extents_end - blocked_extents,
          sizeof(blocked_extents), compare_extents);
    return {FAT_START_INDEX, blocked_extents, blocked_extents_end};
}

uint8_t *allocate_block(block_allocator& all) {
    auto *fat = reinterpret_cast<uint32_t *>(meta_info.fat_start);
    while (all.entry_index != meta_info.fat_entries) {
        while (all.blocked_current != all.blocked_end &&
               all.entry_index >= all.blocked_current->start_index) {
            all.entry_index = max(all.entry_index, all.blocked_current->end_index);
            all.blocked_current++;
        }

        all.entry_index++;
    }

    uint8_t *block = all.entry_index >= meta_info.fat_entries
           ? nullptr
           : meta_info.data_start + all.entry_index * meta_info.cluster_size;
    ++all.entry_index;
    return block;
}
