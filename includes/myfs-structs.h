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
    string content;                 // Data
    __uid_t uid;                    // User ID
    __gid_t gid;                    // Gruppen ID
    __mode_t mode;                  // File mode
    __time_t  atime;          // Time of last access.
    __time_t  mtime;          // Time of last modification.
    __time_t  ctime;          // Time of last status change.
};

#endif /* myfs_structs_h */
