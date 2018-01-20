#ifndef OFS_CONVERT_BLOCK_ALLOCATE_H
#define OFS_CONVERT_BLOCK_ALLOCATE_H

#include <stdint.h>

#include "fat.h"

struct extent_allocator {
    uint32_t index_in_fat,
             blocked_extent_count;
    extent *blocked_extents, *blocked_extent_current;
};
extern extent_allocator allocator;

void init_extent_allocator();
extent allocate_extent(uint32_t& remaining_count);
uint32_t find_blocked_extents(uint32_t physical_start);

#endif //OFS_CONVERT_BLOCK_ALLOCATE_H
