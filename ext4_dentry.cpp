#include "fat.h"
#include "ext4.h"
#include "ext4_extent.h"
#include "ext4_dentry.h"
#include "extent-allocator.h"
#include "stream-archiver.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

// Adapted from https://www.cprogramming.com/tutorial/utf8.c
int ucs2toutf8(uint8_t *dest, uint8_t *dest_end, uint16_t *src, int src_size) {
    uint16_t ch;
    for (int i = 0; src[i]!=0 && i < src_size; i++) {
        ch = src[i];
        if (ch < 0x80) {
            if (dest >= dest_end)
                return dest_end - dest;
            *dest++ = (char)ch;
        } else if (ch < 0x800) {
            if (dest >= dest_end-1)
                return dest_end - dest;
            *dest++ = (ch>>6) | 0xC0;
            *dest++ = (ch & 0x3F) | 0x80;
        } else {
            if (dest >= dest_end-2)
                return dest_end - dest;
            *dest++ = (ch>>12) | 0xE0;
            *dest++ = ((ch>>6) & 0x3F) | 0x80;
            *dest++ = (ch & 0x3F) | 0x80;
        }
    }
    return dest_end - dest;
}

struct ext4_dentry *build_dentry(uint32_t inode_number, StreamArchiver *read_stream) {
    ext4_dentry *ext_dentry = (ext4_dentry *) malloc(sizeof *ext_dentry);
    ext_dentry->inode = inode_number;
    ext_dentry->name_len = 0;

    uint8_t *ext_name = ext_dentry->name;
    uint8_t *ext_name_limit = ext_name + EXT4_MAX_NAME_LENGTH;
    uint16_t *segment = (uint16_t *) iterateStreamArchiver(read_stream, false,
                                                           LFN_ENTRY_LENGTH * sizeof *segment);
    while (segment != NULL) {
        int bytes_written = ucs2toutf8(ext_name + ext_dentry->name_len, ext_name_limit,
                                       segment, LFN_ENTRY_LENGTH);
        ext_dentry->name_len += bytes_written;
        segment = (uint16_t *) iterateStreamArchiver(read_stream, false,
                                                     LFN_ENTRY_LENGTH * sizeof *segment);
    }
    ext_dentry->rec_len = ext_dentry->name_len + 8;
    return ext_dentry;
}

ext4_dentry build_special_dentry(uint32_t inode_no, const char *name) {
    ext4_dentry dentry;
    dentry.inode = inode_no;
    dentry.name_len = strlen(name);
    memcpy(dentry.name, name, dentry.name_len);
    dentry.rec_len = dentry.name_len + 8;
    return dentry;
}

ext4_dentry build_dot_dir_dentry(uint32_t dir_inode_no) {
    return build_special_dentry(dir_inode_no, ".");
}

ext4_dentry build_dot_dot_dir_dentry(uint32_t parent_inode_no) {
    return build_special_dentry(parent_inode_no, "..");
}

ext4_dentry build_lost_found_dentry() {
    return build_special_dentry(EXT4_LOST_FOUND_INODE, "lost+found");
}
