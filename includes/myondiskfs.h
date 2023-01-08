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

    int readRoot(int fd) {

        // Clear the root map before it gets read
        root.clear();

        // Read the blocks of the Root to the file system
        for (int i = 0; i < NUM_DIR_ENTRIES; i++) {

            // Allocate a buffer for the FAT
            char *buffer = (char*) malloc(BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);

            // Write the entries to the buffer
            this->blockDevice->read(i + superBlock.rootBlockOffset, buffer);

            // Copy the buffer into the entries for this block
            MyFsDiskInfo file;
            memcpy(&file, buffer, sizeof(MyFsDiskInfo));

            // Write the key with a slash for easier path finding
            if(strcmp(file.name, "") != 0) {
                string key = "/";
                key.append(file.name);
                root.emplace(key, file);
            }

            // Free the buffer
            free(buffer);
        }

    }

    int writeRoot(int fd) {

        // Clear the root container before it gets written
        for (int i = 0; i < NUM_DIR_ENTRIES; ++i) {

            // Allocate a buffer for the Root
            char *buffer = (char*) malloc(BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);

            // Clear the block
            this->blockDevice->write(i + superBlock.rootBlockOffset, buffer);

            // Free the buffer
            free(buffer);
        }

        // Write root
        size_t index = 0;
        for (const auto& entry : root) {

            // Allocate a buffer for the Root
            char *buffer = (char*) malloc(BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);

            // Copy the entries for this block into the buffer
            memcpy(buffer, &entry.second, sizeof(MyFsDiskInfo));

            // Write the entries to the block
            this->blockDevice->write(index + superBlock.rootBlockOffset, buffer);

            // Free the buffer
            free(buffer);

            index++;

            if(index >= NUM_DIR_ENTRIES)
                break;
        }
    }

};

#endif //MYFS_MYONDISKFS_H
