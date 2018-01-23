#include "extent-allocator.h"
#include "metadata_reader.h"
#include "partition.h"
#include "stream-archiver.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, const char** argv) {
    if (argc < 2) {
        printf("Wrong usage\n");
        exit(1);
    }
    Partition partition = {.path = argv[1]};
    if (!openPartition(&partition)) {
        fprintf(stderr, "Failed to open partition");
        return 1;
    }

    read_boot_sector(partition.ptr);
    set_meta_info(partition.ptr);
    init_extent_allocator();

    StreamArchiver stream;
    init_stream_archiver(&stream);
    StreamArchiver ext_stream = stream;
    aggregate_extents(boot_sector.root_cluster_no, &stream);
    traverse(&ext_stream, &stream);
}
