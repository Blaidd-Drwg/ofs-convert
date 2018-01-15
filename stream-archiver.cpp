#include <stream-archiver.h>

void cutStreamArchiver(StreamArchiver* stream) {
    stream->header = reinterpret_cast<StreamArchiverHeader*>(addToStreamArchiver(stream, sizeof(StreamArchiverHeader), 0));
    stream->header.elementCount = 0;
}

void* insertInStreamArchiver(StreamArchiver* stream, uint64_t elementLength, uint64_t elementCount = 1) {
    *stream->header.elementCount += elementCount;
    auto offsetInPage = stream->offsetInPage;
    stream->offsetInPage += elementLength;
    if(stream->offsetInPage > pageSize) {
        Page* page = malloc(pageSize);
        stream->page.next = page;
        stream->page = page;
        stream->offsetInPage = offsetInPage = sizeof(StreamArchiverHeader);
    }
    return reinterpret_cast<uint8_t*>(stream->page) + offsetInPage;
}
