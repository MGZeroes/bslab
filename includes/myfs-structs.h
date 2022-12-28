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

#include <vector>

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

    // File metadata
    size_t size; // Data Size   64bit
    int32_t data; // Block Pos  32bit
    __uid_t uid; // User ID     32bit
    __gid_t gid; // Gruppen ID  32bit
    __mode_t mode; // File mode 32bit
    __time_t atime; // Time of last access.         64bit
    __time_t mtime; // Time of last modification.   64bit
    __time_t ctime; // Time of last status change.  64bit
};

struct SuperBlock {
    uint32_t blockSize = BLOCK_SIZE; // Size of a block in bytes
    uint32_t numBlocks = 32;        // Total number of blocks in the file system
    uint32_t numFreeBlocks = 29;    // Number of free blocks in the file system
    uint8_t dmapBlock = 1;          // Block number of the data map
    uint8_t fatBlock = 2;           // Block number of the file allocation table
    uint8_t rootBlock = 3;          // Block number of the root directory
};

#endif /* myfs_structs_h */
