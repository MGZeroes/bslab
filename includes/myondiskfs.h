//
// Created by Oliver Waldhorst on 20.03.20.
// Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

#ifndef MYFS_MYONDISKFS_H
#define MYFS_MYONDISKFS_H

#include <unistd.h>
#include <cstring>
#include <array>
#include <map>
#include <unordered_set>
#include <iterator>

#include "myfs.h"

/// @brief On-disk implementation of a simple file system.
class MyOnDiskFS : public MyFS {
protected:
    // BlockDevice blockDevice;

    SuperBlock superBlock;
    array<DMapEntry, FILE_BLOCK_COUNT> dmap;
    array<FATEntry, FILE_BLOCK_COUNT> fat;
    map<string, MyFsDiskInfo> root;
    unordered_set<string> openFiles;

public:
    static MyOnDiskFS *Instance();

    // TODO: [PART 1] Add attributes of your file system here

    MyOnDiskFS();
    ~MyOnDiskFS();

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

    // TODO: Add methods of your file system here
private:

    int readSuperblock(int fd) {

        int ret;
        char *buffer = (char *) malloc(BLOCK_SIZE);

        // Read Superblock
        ret = this->blockDevice->read(0, buffer);
        memcpy(&superBlock, buffer, sizeof(SuperBlock));
        free(buffer);

        return ret;
    }

    int writeSuperblock(int fd) {

        char *buffer = (char*) malloc(BLOCK_SIZE);

        //write superblock
        memset(buffer, 0, BLOCK_SIZE);
        //LOG("cleared buffer");
        memcpy(buffer, &superBlock, sizeof(SuperBlock));
        this->blockDevice->write(0, buffer);
        free(buffer);

        return 0;
    }

    int readDmap(int fd) {

        // Read the blocks of the DMAP to the file system
        for (int i = 0; i < DMAP_BLOCK_COUNT; i++) {

            // Allocate a buffer for the DMAP
            char *buffer = (char*) malloc(BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);

            // Write the entries to the buffer
            this->blockDevice->read(i + superBlock.dmapBlockOffset, buffer);

            // Calculate the start index of the entries in this block
            int startIndex = i * DMAP_ENTRIES_PER_BLOCK;
            size_t bytes = sizeof(DMapEntry) * DMAP_ENTRIES_PER_BLOCK;

            // Copy the buffer into the entries for this block
            memcpy(&dmap[startIndex], buffer, bytes);

            // Free the buffer
            free(buffer);
        }

        return 0;
    }

    int writeDmap(int fd) {

        // Write the blocks of the DMAP to the file system
        for (int i = 0; i < DMAP_BLOCK_COUNT; i++) {

            // Allocate a buffer for the DMAP
            char *buffer = (char*) malloc(BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);

            // Calculate the start index of the entries in this block
            int startIndex = i * DMAP_ENTRIES_PER_BLOCK;
            size_t bytes = sizeof(DMapEntry) * DMAP_ENTRIES_PER_BLOCK;

            // Copy the entries for this block into the buffer
            memcpy(buffer, &dmap[startIndex], bytes);

            // Write the entries to the block
            this->blockDevice->write(i + superBlock.dmapBlockOffset, buffer);

            // Free the buffer
            free(buffer);
        }

        return 0;
    }

    int readFat(int fd) {

        // Read the blocks of the FAT to the file system
        for (int i = 0; i < FAT_BLOCK_COUNT; i++) {

            // Allocate a buffer for the FAT
            char *buffer = (char*) malloc(BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);

            // Write the entries to the buffer
            this->blockDevice->read(i + superBlock.fatBlockOffset, buffer);

            // Calculate the start index of the entries in this block
            int startIndex = i * FAT_ENTRIES_PER_BLOCK;
            size_t bytes = sizeof(FATEntry) * FAT_ENTRIES_PER_BLOCK;

            // Copy the buffer into the entries for this block
            memcpy(&fat[startIndex], buffer, bytes);

            // Free the buffer
            free(buffer);
        }

        return 0;
    }

    int writeFat(int fd) {

        // Write the blocks of the FAT to the file system
        for (int i = 0; i < FAT_BLOCK_COUNT; i++) {

            // Allocate a buffer for the FAT
            char *buffer = (char*) malloc(BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);

            // Calculate the start index of the entries in this block
            int startIndex = i * FAT_ENTRIES_PER_BLOCK;
            size_t bytes = sizeof(FATEntry) * FAT_ENTRIES_PER_BLOCK;

            // Copy the entries for this block into the buffer
            memcpy(buffer, &fat[startIndex], bytes);

            // Write the entries to the block
            this->blockDevice->write(i + superBlock.fatBlockOffset, buffer);

            // Free the buffer
            free(buffer);
        }

        return 0;
    }

};

#endif //MYFS_MYONDISKFS_H
