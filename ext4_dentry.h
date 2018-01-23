#ifndef OFS_EXT4_DENTRY_H
#define OFS_EXT4_DENTRY_H
#include <stdint.h>

struct StreamArchiver;

struct ext4_dentry {
    uint32_t inode;     /* Inode number */
    uint16_t rec_len;   /* Directory entry length */
    uint16_t name_len;  /* Name length */
    uint8_t  name[];    /* File name */
};

struct ext4_dentry *build_dentry(uint32_t inode_number, StreamArchiver *read_stream);

#endif //OFS_EXT4_DENTRY_H
