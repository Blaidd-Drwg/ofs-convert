#include "extent-allocator.h"
#include <stdlib.h>

extent_allocator allocator;

int extent_sort_compare(const void* eA, const void* eB) {
    return reinterpret_cast<const fat_extent*>(eA)->physical_start
         - reinterpret_cast<const fat_extent*>(eB)->physical_start;
}

void init_extent_allocator(fat_extent *blocked_extents) {
    allocator.index_in_fat = FAT_START_INDEX;
    allocator.blocked_extent_current = blocked_extents;
    qsort(allocator.blocked_extents, allocator.blocked_extent_count, sizeof(fat_extent), extent_sort_compare);
}

bool can_be_used() {
    if(allocator.index_in_fat < allocator.blocked_extent_current->physical_start)
        return is_free_cluster(*fat_entry(allocator.index_in_fat++));
    allocator.index_in_fat = allocator.blocked_extent_current->physical_start + allocator.blocked_extent_current->length;
    ++allocator.blocked_extent_current;
    return false;
}

fat_extent allocate_extent(uint16_t max_length) {
    while(!can_be_used());
    fat_extent result;
    result.physical_start = allocator.index_in_fat;
    while(can_be_used()) {
        result.length = allocator.index_in_fat - result.physical_start;
        if(result.length == max_length)
            break;
    }
    return result;
}

uint32_t find_first_blocked_extent(uint32_t physical_address) {
    uint32_t begin = 0, mid, end = allocator.blocked_extent_count;
    while(begin < end) {
        mid = (begin+end)/2;
        fat_extent* blocked_extent = &allocator.blocked_extents[mid];
        if(blocked_extent->physical_start + blocked_extent->length < physical_address)
            begin = mid+1;
        else
            end = mid;
    }
    return begin;
}

fat_extent* find_next_blocked_extent(uint32_t& i, uint32_t physical_end) {
    if(i >= allocator.blocked_extent_count)
        return NULL;
    fat_extent* blocked_extent = &allocator.blocked_extents[i++];
    if(physical_end < blocked_extent->physical_start)
        return NULL;
    return blocked_extent;
}
