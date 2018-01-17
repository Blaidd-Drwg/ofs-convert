#include "stream-archiver.h"
#include <stdlib.h>
#include <string.h>

void cutStreamArchiver(StreamArchiver* stream) {
    if(stream->header && stream->page)
        stream->header->elementCount = stream->elementIndex;
    else {
        stream->page = reinterpret_cast<Page*>(malloc(pageSize));
        stream->page->next = NULL;
        stream->offsetInPage = sizeof(Page);
    }
    stream->elementIndex = 0;
    stream->header = reinterpret_cast<StreamArchiver::Header*>(iterateStreamArchiver(stream, true, sizeof(StreamArchiver::Header), 0));
}

void* iterateStreamArchiver(StreamArchiver* stream, bool insert, uint64_t elementLength, uint64_t elementCount) {
    stream->elementIndex += elementCount;
    if(!insert && elementCount > 0 && stream->elementIndex > stream->header->elementCount) {
        stream->elementIndex = 0;
        stream->header = reinterpret_cast<StreamArchiver::Header*>(iterateStreamArchiver(stream, insert, sizeof(StreamArchiver::Header), 0));
        return NULL;
    }
    uint64_t offsetInPage = stream->offsetInPage;
    if(stream->offsetInPage + elementLength > pageSize) {
        if(insert) {
            Page* page = reinterpret_cast<Page*>(malloc(pageSize));
            stream->page->next = page;
            stream->page = page;
            stream->page->next = NULL;
        } else
            stream->page = stream->page->next;
        offsetInPage = sizeof(Page);
    }
    stream->offsetInPage = offsetInPage + elementLength;
    return reinterpret_cast<uint8_t*>(stream->page) + offsetInPage;
}
