#include "fat.h"
#include "partition.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
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
    recursive_traverse(boot_sector.root_cluster_no, NULL);
}
