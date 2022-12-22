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

#include <string>

using namespace std;

// TODO: Add structures of your file system here
struct MyFsMemoryInfo {
    char name[NAME_LENGTH + 1];     // Path to the file    256bit
    size_t size;                    // Data Size    64bit
    string content;                     // Data
    __uid_t uid;                    // User ID      32bit
    __gid_t gid;                    // Gruppen ID   32bit
    __mode_t mode;                  // File mode    32bit
    __time_t atime;                 // Time of last access.         64bit
    __time_t mtime;                 // Time of last modification.   64bit
    __time_t ctime;                 // Time of last status change.  64bit
};

#endif /* myfs_structs_h */
