//
// Created by Oliver Waldhorst on 20.03.20.
// Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

#ifndef MYFS_MYINMEMORYFS_H
#define MYFS_MYINMEMORYFS_H

#include <fuse.h>
#include <cmath>
#include <string>
#include <map>
#include <unordered_set>

#include "myfs.h"
#include "blockdevice.h"
#include "myfs-structs.h"

using namespace std;

/// @brief In-memory implementation of a simple file system.
class MyInMemoryFS : public MyFS {
protected:
    // BlockDevice blockDevice;

public:
    static MyInMemoryFS *Instance();

    map<string, MyFsMemoryInfo> files;
    unordered_set<string> openFiles;


    MyInMemoryFS();
    ~MyInMemoryFS();

    static void SetInstance();

    // --- Methods called by FUSE ---
    // For Documentation see https://libfuse.github.io/doxygen/structfuse__operations.html
    virtual int fuseGetattr(const char *path, struct stat *statbuf);
    virtual int fuseMknod(const char *path, mode_t mode, dev_t dev);
    virtual int fuseUnlink(const char *path);
    virtual int fuseRename(const char *path, const char *newpath);
    virtual int fuseChmod(const char *path, mode_t mode);
    virtual int fuseChown(const char *path, uid_t uid, gid_t gid);
    virtual int fuseTruncate(const char *path, off_t newSize);
    virtual int fuseOpen(const char *path, struct fuse_file_info *fileInfo);
    virtual int fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
    virtual int fuseWrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
    virtual int fuseRelease(const char *path, struct fuse_file_info *fileInfo);
    virtual void* fuseInit(struct fuse_conn_info *conn);
    virtual int fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo);
    virtual int fuseTruncate(const char *path, off_t offset, struct fuse_file_info *fileInfo);
    virtual void fuseDestroy();

};

#endif //MYFS_MYINMEMORYFS_H
