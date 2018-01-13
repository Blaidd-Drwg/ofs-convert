#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#ifdef __APPLE__
#define MMAP_FUNC mmap
#else
#define MMAP_FUNC mmap64
#endif

uint8_t* openPartition(const char* path) {
    int mmapFlags, file;
    struct stat fileStat;

    if(strcmp(path, "/dev/zero") == 0) {
        mmapFlags |= MAP_PRIVATE|MAP_ANON;
        file = -1;
    } else {
        mmapFlags |= MAP_SHARED|MAP_FILE;
        file = open(path, O_RDWR|O_CREAT, 0666);
        if(file < 0) {
            fprintf(stderr, "Could not open path.\n");
            return NULL;
        }
        assert(fstat(file, &fileStat) == 0);
        if(S_ISREG(fileStat.st_mode)) {
            // if(fileStat.st_size == 0)
            //     assert(ftruncate(file, ) == 0);
        } else if(S_ISBLK(fileStat.st_mode) || S_ISCHR(fileStat.st_mode)) {

        } else {
            fprintf(stderr, "Path must be \"/dev/zero\", a file or a device.\n");
            return NULL;
        }
    }

    uint8_t* ptr = reinterpret_cast<uint8_t*>(MMAP_FUNC(0, fileStat.st_size, PROT_READ|PROT_WRITE, mmapFlags, file, 0));
    // mmapFlags |= MAP_FIXED;
    close(file);
    return ptr;
}
