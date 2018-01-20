#ifndef OFS_CONVERT_BLOCK_ALLOCATE_H
#define OFS_CONVERT_BLOCK_ALLOCATE_H

#include <stdint.h>

#include "fat.h"

struct extent_allocator {
    uint32_t index_in_fat,
             blocked_extent_count;
    extent* blocked_extents;
};

void init_extent_allocator(extent_allocator& allocator);

extent allocate_extent(extent_allocator& allocator, uint32_t& remaining_count);

#endif //OFS_CONVERT_BLOCK_ALLOCATE_H
