#include "extent-allocator.h"
#include <stdlib.h>

extent_allocator allocator;

int extent_sort_compare(const void* eA, const void* eB) {
    return reinterpret_cast<const extent*>(eA)->physical_start
         - reinterpret_cast<const extent*>(eB)->physical_start;
}

void init_extent_allocator() {
    allocator.index_in_fat = FAT_START_INDEX;
    allocator.blocked_extent_current = allocator.blocked_extents;
    qsort(allocator.blocked_extents, allocator.blocked_extent_count, sizeof(extent), extent_sort_compare);
}

bool can_be_used() {
    if(allocator.index_in_fat < allocator.blocked_extent_current->physical_start)
        return is_free_cluster(*fat_entry(allocator.index_in_fat++));
    allocator.index_in_fat = allocator.blocked_extent_current->physical_start + allocator.blocked_extent_current->length;
    ++allocator.blocked_extent_current;
    return false;
}

extent allocate_extent(uint32_t& remaining_count) {
    while(!can_be_used());
    extent result;
    result.physical_start = allocator.index_in_fat;
    while(can_be_used());
    result.length = allocator.index_in_fat - result.physical_start;
    remaining_count -= result.length;
    return result;
}

uint32_t find_blocked_extents(uint32_t physical_start) {
    uint32_t begin = 0, mid, end = allocator.blocked_extent_count;
    while(begin < end) {
        mid = (begin+end)/2;
        if(allocator.blocked_extents[mid].physical_start < physical_start)
            begin = mid+1;
        else
            end = mid;
    }
    return begin;
}
