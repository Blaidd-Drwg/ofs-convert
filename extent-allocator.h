#ifndef OFS_CONVERT_BLOCK_ALLOCATE_H
#define OFS_CONVERT_BLOCK_ALLOCATE_H

#include <stdint.h>

#include "fat.h"


struct cluster_extent {
    uint32_t start_cluster;
    uint32_t end_cluster;
};

struct extent_allocator {
    uint32_t entry_index;
    const cluster_extent *blocked_current;
    const cluster_extent *blocked_end;
};

extent_allocator make_extent_allocator(cluster_extent *blocked_extents,
                                       cluster_extent *blocked_extents_end);

cluster_extent allocate_extent(extent_allocator& allocator,
                               uint32_t max_length);

#endif //OFS_CONVERT_BLOCK_ALLOCATE_H
