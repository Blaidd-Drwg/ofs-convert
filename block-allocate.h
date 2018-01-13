#ifndef OFS_CONVERT_BLOCK_ALLOCATE_H
#define OFS_CONVERT_BLOCK_ALLOCATE_H

#include <stdint.h>

#include "fat.h"


struct cluster_extent {
    uint32_t start_index;
    uint32_t end_index;
};

struct block_allocator {
    uint32_t entry_index;
    const cluster_extent *blocked_current;
    const cluster_extent *blocked_end;
};

block_allocator make_block_allocator(cluster_extent *blocked_extents,
                                     cluster_extent *blocked_extents_end);

uint8_t *allocate_block(block_allocator& allocator);

#endif //OFS_CONVERT_BLOCK_ALLOCATE_H
