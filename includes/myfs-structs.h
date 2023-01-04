//
//  myfs-structs.h
//  myfs
//
//  Created by Oliver Waldhorst on 07.09.17.
//  Copyright © 2017 Oliver Waldhorst. All rights reserved.
//

#ifndef myfs_structs_h
#define myfs_structs_h

#define NAME_LENGTH 255
#define BLOCK_SIZE 512
#define NUM_DIR_ENTRIES 64
#define NUM_OPEN_FILES 64


#define DISK_SIZE 33554432      // 2^25 (33.554432 MB)
#define FILE_BLOCK_COUNT 65536  // DISK_SIZE / BLOCK_SIZE

#define SUPERBLOCK_BLOCK_COUNT 1
#define DMAP_BLOCK_COUNT 1
#define FAT_BLOCK_COUNT 512
#define FAT_ENTRIES_PER_BLOCK 128
#define ROOT_BLOCK_COUNT 64
#define MAX_BLOCK_COUNT 66242 // 33915904 B (33.915904 MB)

#include <vector>
#include <limits>

using namespace std;

// TODO: Add structures of your file system here
struct MyFsMemoryInfo {

    // File data
    vector<char> content;

    // File metadata
    __uid_t uid; // User ID
    __gid_t gid; // Group ID
    __mode_t mode; // File mode
    __time_t  atime; // Time of last access
    __time_t  mtime; // Time of last modification
    __time_t  ctime; // Time of last status change
};

struct MyFsDiskInfo {
    char name[NAME_LENGTH];  // File name
    size_t size;    // File size            64bit
    uint16_t data;  // First block allocated to the file  32bit
    __uid_t uid;    // Owner user ID        32bit
    __gid_t gid;    // Owner group ID       32bit
    __mode_t mode;  // File permissions     32bit
    __time_t atime; // Last accessed time   64bit
    __time_t mtime; // Last modified time   64bit
    __time_t ctime; // Creation time        64bit
};

struct SuperBlock {
    uint32_t blockSize = BLOCK_SIZE; // Size of a block in bytes
    uint32_t numBlocks = MAX_BLOCK_COUNT; // Total number of blocks in the file system
    uint16_t numFreeBlocks = numeric_limits<uint16_t>::max();    // Number of free blocks in the file system
    uint8_t dmapBlockOffset = 1;          // Block number of the data map
    uint8_t fatBlockOffset = 2;           // Block number of the file allocation table
    uint8_t rootBlockOffset = 514;          // Block number of the root directory
};

struct DMapBlock {
    uint16_t numFreeBlocks = numeric_limits<uint16_t>::max();     // Number of free blocks in this dmap block
    uint16_t bitmap = numeric_limits<uint16_t>::max();  // Bitmap of free blocks in this dmap block
};

struct FATEntry {
    uint16_t nextBlock = 0; // Block number of the next block in the file
    bool isLast = true;     // Flag indicating whether this is the last block in the file
};

#endif /* myfs_structs_h */
