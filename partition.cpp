#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdint.h>

#include "partition.h"

#ifdef __APPLE__
#define MMAP_FUNC mmap
#else
#define MMAP_FUNC mmap64
#endif

bool openPartition(Partition* partition) {
    if(strcmp(partition->path, "/dev/zero") == 0) {
        partition->mmapFlags |= MAP_PRIVATE|MAP_ANON;
        partition->file = -1;
    } else {
        partition->mmapFlags |= MAP_SHARED|MAP_FILE;
        partition->file = open(partition->path, O_RDWR|O_CREAT, 0666);
        if(partition->file < 0) {
            perror("open");
            return false;
        }
        if (fstat(partition->file, &partition->fileStat)) {
            perror("fstat");
            return false;
        }
        if(S_ISREG(partition->fileStat.st_mode)) {
            // if(partition->fileStat.st_size == 0)
            //     assert(ftruncate(partition->file, ) == 0);
        } else if(S_ISBLK(partition->fileStat.st_mode) || S_ISCHR(partition->fileStat.st_mode)) {

        } else {
            fprintf(stderr, "Path must be \"/dev/zero\", a file or a device.\n");
            return false;
        }
    }

    partition->ptr = reinterpret_cast<uint8_t*>(MMAP_FUNC(0, partition->fileStat.st_size, PROT_READ|PROT_WRITE, partition->mmapFlags, partition->file, 0));
    assert(partition->ptr != MAP_FAILED);
    return true;
}

void closePartition(Partition* partition) {
    if (munmap(partition->ptr, partition->fileStat.st_size)) {
        perror("munmap");
    }

    close(partition->file);
    partition->file = -1;
    partition->ptr = NULL;
}
