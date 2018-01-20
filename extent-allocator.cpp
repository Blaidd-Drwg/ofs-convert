#include "extent-allocator.h"
#include <stdlib.h>

bool can_be_used(extent_allocator& allocator) {
    if(allocator.index_in_fat < allocator.blocked_extents->physical_start)
        return is_free_cluster(meta_info.fat_start[allocator.index_in_fat++]);
    allocator.index_in_fat = allocator.blocked_extents->physical_start + allocator.blocked_extents->length;
    ++allocator.blocked_extents;
    return false;
}

int extent_sort_compare(const void* eA, const void* eB) {
    return reinterpret_cast<const extent*>(eA)->physical_start
         - reinterpret_cast<const extent*>(eB)->physical_start;
}

void init_extent_allocator(extent_allocator& allocator) {
    allocator.index_in_fat = FAT_START_INDEX;
    qsort(allocator.blocked_extents, allocator.blocked_extent_count, sizeof(extent), extent_sort_compare);
    while(!can_be_used(allocator));
}

extent allocate_extent(extent_allocator& allocator, uint32_t& remaining_count) {
    extent result;
    result.physical_start = allocator.index_in_fat;
    while(can_be_used(allocator));
    result.length = allocator.index_in_fat - result.physical_start;
    remaining_count -= result.length;
    while(!can_be_used(allocator));
    return result;
}
