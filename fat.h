#ifndef OFS_CONVERT_FAT_H
#define OFS_CONVERT_FAT_H


constexpr uint32_t CLUSTER_ENTRY_MASK = 0x0FFFFFFF;
constexpr uint32_t FREE_CLUSTER = 0;


struct meta_info {
    char* fat_start;
    uint16_t fat_entries;
    uint32_t cluster_size;
    char* data_start;
};


#endif //OFS_CONVERT_FAT_H
