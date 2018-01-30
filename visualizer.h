#ifndef OFS_CONVERT_VISUALIZER_H
#define OFS_CONVERT_VISUALIZER_H

#include <stdlib.h>

struct BlockRange {
    enum Type {
        Payload
    } type;
    uint32_t begin, length;
    BlockRange* next;
};

void visualizer_add_block_range(BlockRange to_add);
void visualizer_render_to_file(const char* path, uint32_t block_count);

#endif //OFS_CONVERT_VISUALIZER_H
