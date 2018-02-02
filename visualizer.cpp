#include "visualizer.h"
#include <stdio.h>
#include <memory.h>

const char* type_colors[] = {
    "yellow",
    "green",
    "red",
    "blue"
};

BlockRange* block_range = NULL;

void visualizer_add_block_range(BlockRange source) {
#ifdef VISUALIZER
    BlockRange* destination = (BlockRange*)malloc(sizeof(BlockRange));
    memcpy(destination, &source, sizeof(BlockRange));
    destination->next = block_range;
    block_range = destination;
    printf("visualizer_add_block_range %llu %llu\n", block_range->begin, block_range->length);
#endif  // VISUALIZER
}

void visualizer_render_to_file(const char* path, uint32_t block_count) {
#ifdef VISUALIZER
    const uint32_t bar_width = 1000;
    FILE* output = fopen(path, "w+");
    if(!output)
        return;

    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n", output);
    fputs("<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n", output);
    fprintf(output, "<svg viewBox=\"0 0 %d 200\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xml:space=\"preserve\">\n", bar_width);

    while(block_range) {
        fprintf(output, "\t<rect x=\"%f\" y=\"0\" width=\"%f\" height=\"100\" fill=\"%s\"/>\n",
            (float)bar_width*block_range->begin/block_count,
            (float)bar_width*block_range->length/block_count,
            type_colors[block_range->type]
        );
        block_range = block_range->next;
    }

    fputs("</svg>\n", output);
    fclose(output);
#endif  // VISUALIZER
}
