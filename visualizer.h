#ifndef OFS_CONVERT_VISUALIZER_H
#define OFS_CONVERT_VISUALIZER_H

#include <stdlib.h>
#include <stdint.h>

struct BlockRange {
    enum Type {
        FAT,
        OriginalPayload,
        ResettledPayload,
        BlockGroupHeader,
        Directory,
        Extent
    } type;
    uint64_t begin, length, tag;
    BlockRange* next;
};

void visualizer_add_block_range(BlockRange to_add);
void visualizer_render_to_file(const char* path, uint32_t block_count);

#endif //OFS_CONVERT_VISUALIZER_H
