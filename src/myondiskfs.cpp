//
// Created by Oliver Waldhorst on 20.03.20.
// Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

#include "myondiskfs.h"

// For documentation of FUSE methods see https://libfuse.github.io/doxygen/structfuse__operations.html

#undef DEBUG

// TODO: Comment lines to reduce debug messages
#define DEBUG
#define DEBUG_METHODS
#define DEBUG_RETURN_VALUES

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "macros.h"
#include "myfs.h"
#include "myfs-info.h"
#include "blockdevice.h"

/// @brief Constructor of the on-disk file system class.
///
/// You may add your own constructor code here.
MyOnDiskFS::MyOnDiskFS() : MyFS() {
    // create a block device object
    this->blockDevice= new BlockDevice(BLOCK_SIZE);

    // TODO: [PART 2] Add your constructor code here

}

/// @brief Destructor of the on-disk file system class.
///
/// You may add your own destructor code here.
MyOnDiskFS::~MyOnDiskFS() {
    // free block device object
    delete this->blockDevice;

    // TODO: [PART 2] Add your cleanup code here

}

/// @brief Create a new file.
///
/// Create a new file with given name and permissions.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode Permissions for file access.
/// \param [in] dev Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseMknod(const char *path, mode_t mode, dev_t dev) {
    LOGM();

    LOGF("--> Creating %s", path);

    readDmap();
    readFat();
    readRoot();

    // Check if the filesystem is full
    if(this->root.size() >= NUM_DIR_ENTRIES) {
        LOG("Filesystem is full");
        RETURN(-ENOSPC);
    }

    // Check length of given filename
    if (strlen(path) - 1 > NAME_LENGTH) {
        LOG("Filename too long");
        RETURN(-EINVAL);
    }

    // Check if a file with the same name already exists
    if(root.find(path) != this->root.end()) {
        LOG("File already exists");
        RETURN(-EEXIST);
    }

    LOG("Create file");
    MyFsDiskInfo file;

    // Strip the slash from the path to get the filename
    strcpy(file.name, path+1);

    file.size = 0;
    file.data = NULL;
    file.gid = getgid();
    file.uid = getuid();
    file.mode = mode;
    file.atime = file.ctime = file.mtime = time(NULL);

    LOG("Adding file to filesystem");
    // Insert the file into the map
    this->root.emplace(path, move(file));

    writeDmap();
    writeFat();
    writeRoot();

    RETURN(0);
}

/// @brief Delete a file.
///
/// Delete a file with given name from the file system.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseUnlink(const char *path) {
    LOGM();

    readDmap();
    readFat();
    readRoot();

    LOGF("--> Deleting %s", path);

    // Check if the file exists
    auto iterator = this->root.find(path);
    if(iterator == this->root.end()) {
        LOG("File does not exist");
        RETURN(-ENOENT);
    }

    // Check if the file has data
    if(iterator->second.size > 0) {
        LOG("Freeing allocated files");
        // Free all blocks that are allocated by this file
        freeBlocks(iterator->second.data, bytesToBlocks(iterator->second.size));
    }

    // Remove the file from the map
    this->root.erase(iterator);

    writeDmap();
    writeFat();
    writeRoot();

    RETURN(0);
}

/// @brief Rename a file.
///
/// Rename the file with with a given name to a new name.
/// Note that if a file with the new name already exists it is replaced (i.e., removed
/// before renaming the file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newpath  New name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseRename(const char *path, const char *newpath) {
    LOGM();

    readRoot();

    LOGF("--> Renaming %s into %s", path, newpath);

    // Check if the old file exists
    auto oldIterator = this->root.find(path);
    if (oldIterator == this->root.end()) {
        LOG("File does not exist");
        RETURN(-ENOENT);
    }

    // Check if the new file already exists
    auto newIterator = this->root.find(newpath);
    if (newIterator != this->root.end()) {
        LOG("File already exists");
        RETURN(-EEXIST);
    }

    // Move the file to the new name in the map
    this->root.emplace(newpath, move(oldIterator->second));

    // Remove the old file from the map
    this->root.erase(oldIterator);

    // Update the change time of the new file
    this->root.find(newpath)->second.ctime = time(NULL);

    writeRoot();

    RETURN(0);
}

/// @brief Get file meta data.
///
/// Get the metadata of a file (user & group id, modification times, permissions, ...).
/// \param [in] path Name of the file, starting with "/".
/// \param [out] statbuf Structure containing the meta data, for details type "man 2 stat" in a terminal.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseGetattr(const char *path, struct stat *statbuf) {
    LOGM();

    LOGF("--> Get the metadata of %s", path);

    readRoot();

    statbuf->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
    statbuf->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
    statbuf->st_atime = time(NULL); // The last "a"ccess of the file/directory is right now

    if (strcmp(path, "/") == 0) {
        statbuf->st_mode = S_IFDIR | 0755;
        statbuf->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
    }
    else if(strlen(path) > 0) {
        LOG("Path length > 0");

        // Check if the file exists
        auto iterator = this->root.find(path);
        if(iterator == this->root.end()) {
            LOG("File does not exist");
            RETURN(-ENOENT);
        }

        statbuf->st_mode = iterator->second.mode;
        statbuf->st_nlink = 1;
        statbuf->st_size = iterator->second.size;

        // The last "a"ccess and "m"odification  of the file is right now
        statbuf->st_atime = iterator->second.atime = time(NULL);
        statbuf->st_mtime = iterator->second.mtime = time(NULL);
    }
    else {
        LOG("Path length <= 0");
        RETURN(-ENOENT);
    }

    writeRoot();

    RETURN(0);
}

/// @brief Change file permissions.
///
/// Set new permissions for a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode New mode of the file.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseChmod(const char *path, mode_t mode) {
    LOGM();

    LOGF("--> Changing permissions of %s", path);

    LOG("Reading root blocks");
    readRoot();

    // Check if the file exists
    auto iterator = this->root.find(path);
    if (iterator == this->root.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Update the mode field
    iterator->second.mode = mode;

    // Update the changed time
    iterator->second.ctime = time(NULL);

    writeRoot();

    RETURN(0);
}

/// @brief Change the owner of a file.
///
/// Change the user and group identifier in the meta data of a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] uid New user id.
/// \param [in] gid New group id.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseChown(const char *path, uid_t uid, gid_t gid) {
    LOGM();

    LOGF("--> Changing the owner of %s", path);

    readRoot();

    // Check if the file exists
    auto iterator = this->root.find(path);
    if (iterator == this->root.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Update the uid and gid fields
    iterator->second.uid = uid;
    iterator->second.gid = gid;

    // Update the changed time
    iterator->second.ctime = time(NULL);

    writeRoot();

    RETURN(0);
}

/// @brief Open a file.
///
/// Open a file for reading or writing. This includes checking the permissions of the current user and incrementing the
/// open file count.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [out] fileInfo Can be ignored in Part 1
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseOpen(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Opening %s", path);

    readRoot();

    // Check how many files are open
    if (this->openFiles.size() > NUM_OPEN_FILES) {
        LOG("Too many open files");
        RETURN(-EMFILE);
    }

    // Check if the file is already open
    if (this->openFiles.find(path) != this->openFiles.end()) {
        LOG("File is already open");
        RETURN(-EPERM);
    }

    // Check if the file exists
    auto iterator = this->root.find(path);
    if (iterator == this->root.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Add the file to the open files set
    this->openFiles.insert(path);

    // Store the first block in the fuse_file_info struct
    fileInfo->fh = iterator->second.data;

    RETURN(0);
}

/// @brief Read from a file.
///
/// Read a given number of bytes from a file starting form a given position.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// Note that the file content is an array of bytes, not a string. I.e., it is not (!) necessarily terminated by '\0'
/// and may contain an arbitrary number of '\0'at any position. Thus, you should not use strlen(), strcpy(), strcmp(),
/// ... on both the file content and buf, but explicitly store the length of the file and all buffers somewhere and use
/// memcpy(), memcmp(), ... to process the content.
/// \param [in] path Name of the file, starting with "/".
/// \param [out] buf The data read from the file is stored in this array. You can assume that the size of buffer is at
/// least 'size'
/// \param [in] size Number of bytes to read
/// \param [in] offset Starting position in the file, i.e., number of the first byte to read relative to the first byte of
/// the file
/// \param [in] fileInfo Can be ignored in Part 1
/// \return The Number of bytes read on success. This may be less than size if the file does not contain sufficient bytes.
/// -ERRNO on failure.
int MyOnDiskFS::fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Reading %s", path);
    readFat();
    readRoot();

    // Check if the file exists
    auto iterator = this->root.find(path);
    if (this->root.find(path) == this->root.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Check if the offset is within the file bounds
    if (offset < 0 || offset >= iterator->second.size) {
        LOG("Offset is not within the file bounds");
        size = 0; // The Number of bytes read
    }

    // Check if the file has blocks to read
    if(iterator->second.size == 0) {
        LOG("File is empty");
        size = 0; // The Number of bytes read
    }

    // Check if we need to read
    if(size > 0) {

        off_t blockOffset = offset / BLOCK_SIZE;
        off_t byteOffset = offset % BLOCK_SIZE;
        size_t numBlocks = ceil((double) (byteOffset + size) / BLOCK_SIZE);

        // Get the first block
        uint16_t firstBlock = iterator->second.data;
        for (int i = 0; i < blockOffset; ++i) {
            // Check if we are in the bounds of the FAT
            if(fat.at(firstBlock).isLast && (i < (blockOffset-1))) {
                LOG("File table overflow");
                RETURN(-ENFILE);
            }

            // Set next block as starting block
            firstBlock = fat.at(firstBlock).nextBlock;
        }

        LOGF("Trying to read %d bytes with an offset of %d bytes", size, offset);
        LOGF("Reading %d file blocks starting from block %d", numBlocks, firstBlock);

        // Allocate a buffer for the read size
        char *buffer = (char*) malloc(size);
        memset(buffer, 0, size);

        // Write the file into the buffer
        readFile(firstBlock, buffer, numBlocks);

        // Write the buffer to the output buffer with offset and size
        memcpy(buf, (buffer + byteOffset), size);
        free(buffer);
    }

    // Update the access time
    iterator->second.atime = time(NULL);

    writeRoot();

    RETURN(size);
}

/// @brief Write to a file.
///
/// Write a given number of bytes to a file starting at a given position.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// Note that the file content is an array of bytes, not a string. I.e., it is not (!) necessarily terminated by '\0'
/// and may contain an arbitrary number of '\0'at any position. Thus, you should not use strlen(), strcpy(), strcmp(),
/// ... on both the file content and buf, but explicitly store the length of the file and all buffers somewhere and use
/// memcpy(), memcmp(), ... to process the content.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] buf An array containing the bytes that should be written.
/// \param [in] size Number of bytes to write.
/// \param [in] offset Starting position in the file, i.e., number of the first byte to read relative to the first byte of
/// the file.
/// \param [in] fileInfo Can be ignored in Part 1 .
/// \return Number of bytes written on success, -ERRNO on failure.
int MyOnDiskFS::fuseWrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Writing %s", path);
    readDmap();
    readFat();
    readRoot();

    // Check if the file exists
    auto iterator = this->root.find(path);
    if (this->root.find(path) == this->root.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Check if the offset is within the file bounds
    if (offset < 0) {
        LOGF("Invalid offset: %d", offset);
        return -EINVAL;
    }

    LOGF("Trying to write %d bytes with an offset of %d bytes", size, offset);

    // Calculate the block number and byte offset
    off_t currentBlockNumber = max((u_long) bytesToBlocks(iterator->second.size), 1UL);
    off_t blockOffset = offset / BLOCK_SIZE;
    off_t byteOffset = offset % BLOCK_SIZE;
    size_t numBlocks = bytesToBlocks(byteOffset + size);

    // Check if we need to allocate more blocks
    if(currentBlockNumber < blockOffset + numBlocks) {
        fuseTruncate(path, offset + size);
    }

    // Get the first block to be written
    uint16_t firstBlock = iterator->second.data;
    for (int i = 0; i < blockOffset; ++i) {
        // Check if we are in the bounds of the FAT
        if(fat.at(firstBlock).isLast && (i < (blockOffset-1))) {
            LOG("File table overflow");
            RETURN(-ENFILE);
        }

        // Set next block as starting block
        firstBlock = fat.at(firstBlock).nextBlock;
    }

    LOGF("Writing %d file blocks starting from block %d", numBlocks, firstBlock);

    // Allocate a buffer for the file blocks
    char *buffer = (char*) malloc(numBlocks * BLOCK_SIZE);
    memset(buffer, 0, numBlocks * BLOCK_SIZE);

    // Read the file into the buffer
    readFile(firstBlock, buffer, numBlocks);

    // Write the input buffer into the write buffer
    memcpy(buffer + byteOffset, buf, size);

    // Write the buffer into the file
    writeFile(firstBlock, buffer, numBlocks);

    // Free the buffer
    free(buffer);

    // Update size of the file
    iterator->second.size = max(offset + size, iterator->second.size);

    // Update the access and modified time
    iterator->second.atime = iterator->second.mtime = time(NULL);

    writeDmap();
    writeFat();
    writeRoot();

    RETURN(size);
}

/// @brief Close a file.
///
/// \param [in] path Name of the file, starting with "/".
/// \param [in] File handel for the file set by fuseOpen.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseRelease(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Closing %s", path);

    // Check if the file exists
    if (this->root.find(path) == this->root.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Remove the file from the open files set
    this->openFiles.erase(path);

    RETURN(0);
}

/// @brief Truncate a file.
///
/// Set the size of a file to the new size. If the new size is smaller than the old size, spare bytes are removed. If
/// the new size is larger than the old size, the new bytes may be random.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newSize New size of the file.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize) {
    LOGM();

    readDmap();
    readFat();
    readRoot();

    LOGF("--> Set the size of %s", path);

    // Check if the file exists
    auto iterator = this->root.find(path);
    if (iterator == this->root.end()) {
        LOG("File does not exist");
        RETURN(-EEXIST);
    }

    LOGF("Change the size from %d to %d", iterator->second.size, newSize);

    // Calculate the new block number
    off_t newBlockNumber = bytesToBlocks(newSize);

    // Check if the file has blocks
    if(iterator->second.size == 0 && newSize > 0) {

        LOG("File is empty. Init first block");

        // Check if enough blocks available
        if (this->superBlock.numFreeBlocks < newBlockNumber) {
            LOG("No space left on device");
            RETURN(-ENOSPC);
        }

        LOGF("Allocate %d block(s)", newBlockNumber);
        iterator->second.data = allocateBlocks(-1, newBlockNumber);
        LOGF("First block is %d", iterator->second.data);

    } else if(newSize > 0) {

        // Calculate the current block number
        off_t oldBlockNumber = bytesToBlocks(iterator->second.size);
        LOGF("Change the required blocks from %d to %d", oldBlockNumber, newBlockNumber);

        // Truncate block number
        if (newBlockNumber < oldBlockNumber) {
            LOG("Free blocks to fit the new size");
            freeBlocks(iterator->second.data, newBlockNumber);

        } else if (newBlockNumber > oldBlockNumber) {

            // Check if enough blocks are available
            if (this->superBlock.numFreeBlocks < (newBlockNumber - oldBlockNumber)) {
                LOG("No space left on device");
                RETURN(-ENOSPC);
            }

            LOG("Allocate blocks to fit the new size");
            allocateBlocks(iterator->second.data, newBlockNumber);

        }

    } else if(newSize == 0 && iterator->second.size > 0) {
        LOG("Freeing all allocated files");
        // Free all blocks that are allocated by this file
        freeBlocks(iterator->second.data, bytesToBlocks(iterator->second.size));
    }

    // Truncate the file size
    iterator->second.size = newSize;

    // Update the changed and modified time
    iterator->second.ctime = iterator->second.mtime = time(NULL);

    writeDmap();
    writeFat();
    writeRoot();

    RETURN(0);
}

/// @brief Truncate a file.
///
/// Set the size of a file to the new size. If the new size is smaller than the old size, spare bytes are removed. If
/// the new size is larger than the old size, the new bytes may be random. This function is called for files that are
/// open.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newSize New size of the file.
/// \param [in] fileInfo Can be ignored in Part 1.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize, struct fuse_file_info *fileInfo) {
    LOGM();

    fuseTruncate(path, newSize);

    RETURN(0);
}

/// @brief Read a directory.
///
/// Read the content of the (only) directory.
/// You do not have to check file permissions, but can assume that it is always ok to access the directory.
/// \param [in] path Path of the directory. Should be "/" in our case.
/// \param [out] buf A buffer for storing the directory entries.
/// \param [in] filler A function for putting entries into the buffer.
/// \param [in] offset Can be ignored.
/// \param [in] fileInfo Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Read the content of the directory %s", path);

    LOG("Reading the Root");
    readRoot();

    LOG("Adding '.' and '..'");
    filler(buf, ".", NULL, 0); // Current Directory
    filler(buf, "..", NULL, 0); // Parent Directory

    // If the user is trying to show the files of the root directory show the following
    if(strcmp(path, "/") == 0) {
        // Add the names of the files in the directory
        for(const auto& iterator : this->root) {
            LOGF("Adding '%s'", iterator.first.c_str());
            filler(buf, (iterator.second.name), NULL, 0);
        }
    }

    RETURN(0);
}

/// Initialize a file system.
///
/// This function is called when the file system is mounted. You may add some initializing code here.
/// \param [in] conn Can be ignored.
/// \return 0.
void* MyOnDiskFS::fuseInit(struct fuse_conn_info *conn) {
    // Open logfile
    this->logFile= fopen(((MyFsInfo *) fuse_get_context()->private_data)->logFile, "w+");
    if(this->logFile == NULL) {
        fprintf(stderr, "ERROR: Cannot open logfile %s\n", ((MyFsInfo *) fuse_get_context()->private_data)->logFile);
    } else {
        // turn of logfile buffering
        setvbuf(this->logFile, NULL, _IOLBF, 0);

        LOG("Starting logging...\n");

        LOG("Using on-disk mode");

        LOGF("Container file name: %s", ((MyFsInfo *) fuse_get_context()->private_data)->contFile);

        int ret = this->blockDevice->open(((MyFsInfo *) fuse_get_context()->private_data)->contFile);

        if(ret >= 0) {
            LOG("Container file does exist, reading");
            readSuperblock();
            readDmap();
            readFat();
            readRoot();

        } else if(ret == -ENOENT) {
            LOG("Container file does not exist, creating a new one");

            ret = this->blockDevice->create(((MyFsInfo *) fuse_get_context()->private_data)->contFile);

            if (ret >= 0) {

                LOG("Initialing the container layout");
                writeSuperblock();
                writeDmap();
                writeFat();
                writeRoot();

                LOG("Initialing the last block in the container file");
                char *buffer = (char*) malloc(BLOCK_SIZE);
                memset(buffer, 0, BLOCK_SIZE);
                this->blockDevice->write(MAX_BLOCK_COUNT-1, buffer);
                free(buffer);

            }
        }

        if(ret < 0) {
            LOGF("ERROR: Access to container file failed with error %d", ret);
        }
     }

    return 0;
}

/// @brief Clean up a file system.
///
/// This function is called when the file system is unmounted. You may add some cleanup code here.
void MyOnDiskFS::fuseDestroy() {
    LOGM();

    // TODO: [PART 2] Implement this!

}

// TODO: [PART 2] You may add your own additional methods here!

// DO NOT EDIT ANYTHING BELOW THIS LINE!!!

/// @brief Set the static instance of the file system.
///
/// Do not edit this method!
void MyOnDiskFS::SetInstance() {
    MyFS::_instance= new MyOnDiskFS();
}
