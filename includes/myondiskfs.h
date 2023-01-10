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
        memcpy(&this->superBlock, buffer, sizeof(SuperBlock));
        free(buffer);

        return ret;
    }

    int writeSuperblock(int fd) {

        char *buffer = (char*) malloc(BLOCK_SIZE);

        //write superblock
        memset(buffer, 0, BLOCK_SIZE);
        //LOG("cleared buffer");
        memcpy(buffer, &this->superBlock, sizeof(SuperBlock));
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
            this->blockDevice->read(i + this->superBlock.dmapBlockOffset, buffer);

            // Calculate the start index of the entries in this block
            int startIndex = i * DMAP_ENTRIES_PER_BLOCK;
            size_t bytes = sizeof(DMapEntry) * DMAP_ENTRIES_PER_BLOCK;

            // Copy the buffer into the entries for this block
            memcpy(&this->dmap[startIndex], buffer, bytes);

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
            memcpy(buffer, &this->dmap[startIndex], bytes);

            // Write the entries to the block
            this->blockDevice->write(i + this->superBlock.dmapBlockOffset, buffer);

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
            this->blockDevice->read(i + this->superBlock.fatBlockOffset, buffer);

            // Calculate the start index of the entries in this block
            int startIndex = i * FAT_ENTRIES_PER_BLOCK;
            size_t bytes = sizeof(FATEntry) * FAT_ENTRIES_PER_BLOCK;

            // Copy the buffer into the entries for this block
            memcpy(&this->fat[startIndex], buffer, bytes);

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
            memcpy(buffer, &this->fat[startIndex], bytes);

            // Write the entries to the block
            this->blockDevice->write(i + this->superBlock.fatBlockOffset, buffer);

            // Free the buffer
            free(buffer);
        }

        return 0;
    }

    int readRoot(int fd) {

        // Clear the root map before it gets read
        this->root.clear();

        // Read the blocks of the Root to the file system
        for (int i = 0; i < NUM_DIR_ENTRIES; i++) {

            // Allocate a buffer for the FAT
            char *buffer = (char*) malloc(BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);

            // Write the entries to the buffer
            this->blockDevice->read(i + this->superBlock.rootBlockOffset, buffer);

            // Copy the buffer into the entries for this block
            MyFsDiskInfo file;
            memcpy(&file, buffer, sizeof(MyFsDiskInfo));

            // Write the key with a slash for easier path finding
            if(strcmp(file.name, "") != 0) {
                string key = "/";
                key.append(file.name);
                this->root.emplace(key, file);
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
            this->blockDevice->write(i + this->superBlock.rootBlockOffset, buffer);

            // Free the buffer
            free(buffer);
        }

        // Write root
        size_t index = 0;
        for (const auto& entry : this->root) {

            // Allocate a buffer for the Root
            char *buffer = (char*) malloc(BLOCK_SIZE);
            memset(buffer, 0, BLOCK_SIZE);

            // Copy the entries for this block into the buffer
            memcpy(buffer, &entry.second, sizeof(MyFsDiskInfo));

            // Write the entries to the block
            this->blockDevice->write(index + this->superBlock.rootBlockOffset, buffer);

            // Free the buffer
            free(buffer);

            index++;

            if(index >= NUM_DIR_ENTRIES)
                break;
        }
    }

    int readFileBlock(uint16_t block, char* buf) {
        // Allocate a buffer for a file block
        char *buffer = (char*) malloc(BLOCK_SIZE);
        memset(buffer, 0, BLOCK_SIZE);

        // Write the content to the buffer
        int ret = this->blockDevice->read(block + this->superBlock.fileBlockOffset, buffer);

        // Write the block buffer to the output buffer
        memcpy(buf, buffer, BLOCK_SIZE);

        // Free the buffer
        free(buffer);

        return ret;
    }

    int readFile(uint16_t block, char* buf, uint16_t numBlocks) {

        // iterate through every block
        for (int i = 0; i < numBlocks; i++) {
            // Write file block into the buffer
            readFileBlock(block, buf + (i * BLOCK_SIZE));
            block = fat.at(block).nextBlock;
        }

        return 0;
    }

    int writeFileBlock(uint16_t block, const char* buf) {
        // Allocate a buffer for a file block
        char *buffer = (char*) malloc(BLOCK_SIZE);
        memset(buffer, 0, BLOCK_SIZE);

        // Copy the content for this block into the buffer
        memcpy(buffer, buf, BLOCK_SIZE);

        // Write the content to the block
        int ret = this->blockDevice->write(block + this->superBlock.fileBlockOffset, buffer);

        // Free the buffer
        free(buffer);

        return ret;
    }

    int writeFile(uint16_t block, const char* buf, uint16_t numBlocks) {

        // iterate through every block
        for (int i = 0; i < numBlocks; i++) {
            // Write file block into the buffer
            writeFileBlock(block, buf + (i * BLOCK_SIZE));
            block = fat.at(block).nextBlock;
        }

        return 0;
    }

    int findFreeBlock(int fd) {

        // Check if a block is available
        if(!this->superBlock.numFreeBlocks) {
            return -ENOSPC; // No space left on device
        }

        // Iterate through the bits of the value
        for (size_t i = 0; i < FILE_BLOCK_COUNT; i++) {
            // Check if the i-th block is free
            if(this->dmap.at(i).isFree)
                return i; // The i-th block is free
        }

        return -ERANGE; // No bits set to 1 found
    }

    uint16_t setBlock(uint16_t block) {
        this->dmap.at(block).isFree = false;
        this->superBlock.numFreeBlocks--;
        return block;
    }

    uint16_t clearBlock(uint16_t block) {
        this->dmap.at(block).isFree = true;
        this->superBlock.numFreeBlocks++;
        return block;
    }

    uint16_t allocateBlocks(int firstBlock, uint16_t numBlocks) {

        // Check if enough blocks are available
        if(this->superBlock.numFreeBlocks < numBlocks) {
            return -ENOSPC; // Not enough space left on device
        }

        uint16_t block = firstBlock;

        // Check if there is a starting block
        if(firstBlock >= 0) {
            // Iterate to the last block
            while(!this->fat.at(block).isLast)
                block = this->fat.at(block).nextBlock;

        } else {
            // Init first block
            block = this->findFreeBlock(0);
            this->fat.at(block).isLast = false;
            firstBlock = this->setBlock(block);
            numBlocks--;
        }

        // Add number of blocks
        for (size_t i = 0; i < numBlocks; ++i) {
            uint16_t freeBlock = this->findFreeBlock(0);
            this->fat.at(block).isLast = false;
            this->fat.at(block).nextBlock = freeBlock;
            block = this->setBlock(freeBlock);
        }

        // Set the last block as last
        this->fat.at(block).isLast = true;

        // Return first block
        return firstBlock;
    }

    int freeBlocks(uint16_t firstBlock, uint16_t numBlocks) {

        uint16_t block = firstBlock;

        // Count the number of allocated blocks
        uint16_t numAllocBlocks = 1;
        while(!this->fat.at(block).isLast) {
            block = this->fat.at(block).nextBlock;
            numAllocBlocks++;
        }

        block = firstBlock;

        // Free number of blocks
        for (size_t i = 0; i < numAllocBlocks; i++) {

            // Check if the last number of blocks are reached
            if(i >= (numAllocBlocks - numBlocks)) {
                this->clearBlock(block);

                // Check if the new last block is reached
            } else if((i+1) == (numAllocBlocks - numBlocks)) {
                this->fat.at(block).isLast = true;
            }

            // Get the next block
            block = this->fat.at(block).nextBlock;
        }
    }

};

#endif //MYFS_MYONDISKFS_H
