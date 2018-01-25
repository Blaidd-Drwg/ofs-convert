#include "ext4.h"
#include "ext4_bg.h"
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

    init_ext4_sb();
    init_ext4_group_descs();
    init_extent_allocator(create_block_group_meta_extents());

    StreamArchiver write_stream;
    init_stream_archiver(&write_stream);
    StreamArchiver extent_stream = write_stream;
    StreamArchiver read_stream = write_stream;

    aggregate_extents(boot_sector.root_cluster_no, &write_stream);
    traverse(&extent_stream, &write_stream);

    //build_ext4_metadata_tree(EXT4_ROOT_INODE, &read_stream);
}
