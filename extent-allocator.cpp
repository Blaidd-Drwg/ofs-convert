#include "extent-allocator.h"

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
    return left->start_cluster - right->start_cluster;
}

extent_allocator make_extent_allocator(cluster_extent *blocked_extents,
                                       cluster_extent *blocked_extents_end) {
    qsort(blocked_extents, blocked_extents_end - blocked_extents,
          sizeof(blocked_extents), compare_extents);
    return {FAT_START_INDEX, blocked_extents, blocked_extents_end};
}

bool is_current_index_blocked(extent_allocator& all) {
    return all.blocked_current != all.blocked_end &&
           all.entry_index >= all.blocked_current->start_cluster;
}

bool advance_index(extent_allocator& all) {
    auto *fat = reinterpret_cast<uint32_t *>(meta_info.fat_start);
    if (all.entry_index >= meta_info.fat_entries) {
        return false;
    }

    if (is_current_index_blocked(all)) {
        do {
            all.entry_index = max(all.entry_index,
                                  all.blocked_current->end_cluster);
            all.blocked_current++;
        } while (is_current_index_blocked(all));

        return false;
    }

    return is_free_cluster(fat[all.entry_index++]);
}

cluster_extent allocate_extent(extent_allocator& all, uint32_t max_length) {
    cluster_extent extent{all.entry_index, 0};
    while (all.entry_index < meta_info.fat_entries) {
        extent.start_cluster = all.entry_index;
        if (advance_index(all)) {
            break;
        }
    }

    for (uint32_t i = 0; i < max_length; i++) {
        // The indices only jump if we encounter a blocked extent.
        // When this happens we exit the loop, therefore we never set a jumped
        // index as `extent.end_cluster`.
        extent.end_cluster = all.entry_index;
        if (!advance_index(all)) {
            break;
        }
    }

    return extent;
}
