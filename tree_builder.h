#include <stdint.h>

struct StreamArchiver;

void build_ext4_root();
void build_lost_found();
void build_ext4_metadata_tree(uint32_t parent_inode_number, StreamArchiver *read_stream);

