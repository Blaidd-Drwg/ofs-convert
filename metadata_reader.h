#include <stdint.h>

struct StreamArchiver;

void init_stream_archiver(StreamArchiver* stream);
void aggregate_extents(uint32_t cluster_no, StreamArchiver* write_stream);
void traverse(StreamArchiver* dir_extent_stream, StreamArchiver* write_stream);

