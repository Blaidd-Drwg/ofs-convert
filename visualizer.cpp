#include "visualizer.h"
#include <stdio.h>
#include <memory.h>
#include <math.h>

const char* type_colors[] = {
    "cyan",
    "green",
    "orange",
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
    const uint32_t line_width = 2048, line_height = 20, line_count = 55;
    FILE* output = fopen(path, "w+");
    if(!output)
        return;
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n", output);
    fputs("<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n", output);
    fprintf(output, "<svg viewBox=\"0 0 %d %d\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xml:space=\"preserve\">\n\t<g>\n", line_width, line_height*line_count+20);
    for(uint32_t i = 0; i < line_count; ++i)
        fprintf(output, "\t\t<path stroke-dasharray=\"5 5\" stroke=\"grey\" d=\"M0,%fH%d\"/>\n", line_height*(i+0.4), line_width);
    fputs("\t</g>\n\t<g shape-rendering=\"crispEdges\">\n", output);
    uint32_t resettled = 0;
    while(block_range) {
        float begin = (float)line_width*line_count*block_range->begin/block_count,
              length = (float)line_width*line_count*block_range->length/block_count;
        uint32_t line = begin/line_width;
        begin -= line*line_width;

        while(true) {
            fprintf(output, "\t\t<rect x=\"%f\" y=\"%d\" width=\"%f\" height=\"%f\" fill=\"%s\" class=\"tag%llu\"/>\n",
                begin,
                line_height*line,
                fmin(length, line_width-begin),
                line_height*0.8,
                type_colors[block_range->type],
                block_range->tag
            );

            if(begin+length <= line_width)
                break;
            length -= fmin(length, line_width-begin);
            begin = 0;
            ++line;
        }

        if(block_range->type == BlockRange::ResettledPayload)
            resettled += block_range->length;
        block_range = block_range->next;
    }
    fputs("\t</g>\n\t<g>\n", output);
    fprintf(output, "\t\t<text x=\"10\" y=\"%d\" font-family=\"Verdana\">Blocks: %d x %d, Resettled: %d</text>\n", line_height*line_count+15, block_count/line_count, line_count, resettled);
    fputs("\t</g>\n\t<script type=\"text/javascript\" xlink:href=\"visualizer.js\"/>\n", output);
    fputs("</svg>\n", output);
    fclose(output);
#endif  // VISUALIZER
}
