#ifndef OFS_CONVERT_SAR_H
#define OFS_CONVERT_SAR_H

#include <stdint.h>

uint64_t pageSize;
struct Page {
    struct Page* next;
};

struct StreamArchiver {
    struct StreamArchiverHeader {
        uint64_t elementCount;
    } *header;
    Page* page;
    uint64_t offsetInPage;
};

void cutStreamArchiver(StreamArchiver* stream);
void* insertInStreamArchiver(StreamArchiver* stream, uint64_t elementLength, uint64_t elementCount = 1);

#endif //OFS_CONVERT_SAR_H
