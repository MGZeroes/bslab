//
// Created by Oliver Waldhorst on 20.03.20.
//  Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

#include "myinmemoryfs.h"

// The functions fuseGettattr(), fuseRead(), and fuseReadDir() are taken from
// an example by Mohammed Q. Hussain. Here are original copyrights & licence:

/**
 * Simple & Stupid Filesystem.
 *
 * Mohammed Q. Hussain - http://www.maastaar.net
 *
 * This is an example of using FUSE to build a simple filesystem. It is a part of a tutorial in MQH Blog with the title
 * "Writing a Simple Filesystem Using FUSE in C":
 * http://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
 *
 * License: GNU GPL
 */

// For documentation of FUSE methods see https://libfuse.github.io/doxygen/structfuse__operations.html

#undef DEBUG

#define DEBUG
#define DEBUG_METHODS
#define DEBUG_RETURN_VALUES

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <vector>
#include <map>

#include "macros.h"
#include "myfs.h"
#include "myfs-info.h"
#include "blockdevice.h"

using namespace std;

/// @brief Constructor of the in-memory file system class.
///
/// You may add your own constructor code here.
MyInMemoryFS::MyInMemoryFS() : MyFS() {}

/// @brief Destructor of the in-memory file system class.
///
/// You may add your own destructor code here.
MyInMemoryFS::~MyInMemoryFS() {}

/// @brief Create a new file.
///
/// Create a new file with given name and permissions.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode Permissions for file access.
/// \param [in] dev Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseMknod(const char *path, mode_t mode, dev_t dev) {
    LOGM();

    LOGF("--> Creating %s\n", path);

    // Check if the filesystem is full
    if(files.size() >= NUM_DIR_ENTRIES) {
        LOG("Filesystem is full");
        RETURN(-ENOSPC);
    }

    // Check length of given filename
    if (strlen(path) - 1 > NAME_LENGTH) {
        LOG("Filename too long");
        RETURN(-EINVAL);
    }

    // Check if a file with the same name already exists
    if(files.find(path) != files.end()) {
        LOG("File already exists");
        RETURN(-EEXIST);
    }

    LOG("Create the file");
    // Create a new MyFsMemoryInfo struct for the file
    MyFsMemoryInfo file;
    file.gid = getgid();
    file.uid = getuid();
    file.mode = mode;
    file.atime = file.ctime = file.mtime = time(NULL);

    LOG("Add file to filesystem");
    // Insert the file into the map
    files.emplace(path, move(file));

    RETURN(0);
}

/// @brief Delete a file.
///
/// Delete a file with given name from the file system.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseUnlink(const char *path) {
    LOGM();

    LOGF("--> Deleting %s\n", path);

    // Check if the file exists
    auto iterator = files.find(path);
    if (iterator == files.end()) {
        LOG("File does not exist");
        RETURN(-ENOENT);
    }

    // Remove the file from the map
    files.erase(iterator);

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
int MyInMemoryFS::fuseRename(const char *path, const char *newpath) {
    LOGM();

    LOGF("--> Renaming %s into %s\n", path, newpath);

    // Check if the old file exists
    auto oldIterator = files.find(path);
    if (oldIterator == files.end()) {
        LOG("File does not exist");
        RETURN(-ENOENT);
    }

    // Check if the new file already exists
    auto newIterator = files.find(newpath);
    if (newIterator != files.end()) {
        LOG("File already exists");
        RETURN(-EEXIST);
    }

    // Create a copy of the FileData struct for the new file
    MyFsMemoryInfo newFile = oldIterator->second;

    // Insert the new file into the map
    files.emplace(newpath, move(newFile));

    // Remove the old file from the map
    files.erase(oldIterator);

    // Update the change time of the new file
    files.find(newpath)->second.ctime = time(nullptr);

    RETURN(0);
}

/// @brief Get file meta data.
///
/// Get the metadata of a file (user & group id, modification times, permissions, ...).
/// \param [in] path Name of the file, starting with "/".
/// \param [out] statbuf Structure containing the meta data, for details type "man 2 stat" in a terminal.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseGetattr(const char *path, struct stat *statbuf) {
    LOGM();

    LOGF("\tAttributes of %s requested\n", path);

    // GNU's definitions of the attributes (http://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html):
    // 		st_uid: 	The user ID of the file’s owner.
    //		st_gid: 	The group ID of the file.
    //		st_atime: 	This is the last access time for the file.
    //		st_mtime: 	This is the time of the last modification to the contents of the file.
    //		st_mode: 	Specifies the mode of the file. This includes file type information (see Testing File Type) and
    //		            the file permission bits (see Permission Bits).
    //		st_nlink: 	The number of hard links to the file. This count keeps track of how many directories have
    //	             	entries for this file. If the count is ever decremented to zero, then the file itself is
    //	             	discarded as soon as no process still holds it open. Symbolic links are not counted in the
    //	             	total.
    //		st_size:	This specifies the size of a regular file in bytes. For files that are really devices this field
    //		            isn’t usually meaningful. For symbolic links this specifies the length of the file name the link
    //		            refers to.

    statbuf->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
    statbuf->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
    statbuf->st_atime = time(NULL); // The last "a"ccess of the file/directory is right now

    if (strcmp( path, "/" ) == 0) {
        statbuf->st_mode = S_IFDIR | 0755;
        statbuf->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
    }
    else if(strlen(path) > 0) {
        LOG("Path length > 0");
        auto iterator = files.find(path);

        if(iterator == files.end()) {
            LOG("File does not exist");
            RETURN(-ENOENT);
        }

        statbuf->st_mode = iterator->second.mode;
        statbuf->st_nlink = 1;
        statbuf->st_size = iterator->second.content.size();
        statbuf->st_mtime = iterator->second.mtime; // The last "m"odification of the file/directory is right now
    }
    else {
        LOG("Path length <= 0");
        RETURN(-ENOENT);
    }

    RETURN(0);
}

/// @brief Change file permissions.
///
/// Set new permissions for a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode New mode of the file.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseChmod(const char *path, mode_t mode) {
    LOGM();

    LOGF("--> Changing permissions of %s\n", path);

    // Check if the file exists
    auto iterator = files.find(path);
    if (iterator == files.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Update the mode field
    iterator->second.mode = mode;

    // Update the changed time
    iterator->second.ctime = time(nullptr);

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
int MyInMemoryFS::fuseChown(const char *path, uid_t uid, gid_t gid) {
    LOGM();

    LOGF("--> Changing the owner of %s\n", path);

    // Check if the file exists
    auto iterator = files.find(path);
    if (iterator == files.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Update the uid and gid fields
    iterator->second.uid = uid;
    iterator->second.gid = gid;

    // Update the changed time
    iterator->second.ctime = time(nullptr);

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
int MyInMemoryFS::fuseOpen(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Opening %s\n", path);

    // Check how many files are open
    if (openFiles.size() > NUM_OPEN_FILES) {
        LOG("Too many open files");
        RETURN(-EMFILE);
    }

    // Check if the file is already open
    if (openFiles.find(path) != openFiles.end()) {
        LOG("File is already open");
        RETURN(-EPERM);
    }

    // Add the file to the open files set
    openFiles.insert(path);

    // Check if the file exists
    auto iterator = files.find(path);
    if (iterator == files.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Store the file data pointer in the fuse_file_info struct
    fileInfo->fh = reinterpret_cast<uint64_t>(&iterator->second.content);

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
int MyInMemoryFS::fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Reading %s\n", path);

    // Find the file in the map and copy its contents into the buffer
    if (files.find(path) == files.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Get a pointer to the file data
    vector<char>* content = reinterpret_cast<vector<char>*>(fileInfo->fh);

    // Check if the offset is within the file bounds
    if (offset < 0 || offset >= content->size()) {
        LOG("Offset is not within the file bounds");
        RETURN(0);  // EOF
    }

    // Read data from the file
    size_t count = min(size, content->size() - offset);
    copy(content->begin() + offset, content->begin() + offset + count, buf);

    // Update the access time
    files.find(path)->second.atime = time(nullptr);

    RETURN(count);
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
int MyInMemoryFS::fuseWrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Writing %s\n", path);

    // Get a pointer to the file data
    vector<char>* content = reinterpret_cast<vector<char>*>(fileInfo->fh);

    // Check if the offset is within the file bounds
    if (offset < 0) {
        LOGF("Invalid offset: %d", offset);
        return -EINVAL;
    }

    // Truncate the file data
    if (offset + size > content->size()) {
        LOG("Truncate file");
        content->resize(offset + size);
    }

    // Write data to the file
    copy(buf, buf + size, content->begin() + offset);

    // Update the modification and changed time
    auto iterator = files.find(path);
    iterator->second.mtime = iterator->second.ctime = time(nullptr);

    RETURN(size);
}

/// @brief Close a file.
///
/// In Part 1 this includes decrementing the open file count.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] fileInfo Can be ignored in Part 1 .
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseRelease(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Removing the file %s\n", path);

    // Check if the file exists
    if (files.find(path) == files.end()) {
        LOG("File does not exists");
        RETURN(-ENOENT);
    }

    // Remove the file from the open files set
    openFiles.erase(path);

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
int MyInMemoryFS::fuseTruncate(const char *path, off_t newSize) {
    LOGM();

    LOGF("--> Set the size of %s\n", path);

    // Check if the file exists
    auto iterator = files.find(path);
    if (iterator == files.end()) {
        LOG("File already exists");
        RETURN(-EEXIST);
    }

    // Truncate the file data
    iterator->second.content.resize(newSize);

    // Update the modification and changed time
    iterator->second.mtime = time(nullptr);
    iterator->second.ctime = time(nullptr);

    return 0;
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
int MyInMemoryFS::fuseTruncate(const char *path, off_t newSize, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Set the size of %s\n", path);

    // Get a pointer to the file data
    vector<char>* content = reinterpret_cast<vector<char>*>(fileInfo->fh);

    // Truncate the file data
    content->resize(newSize);

    // Update the modification and changed time
    auto iterator = files.find(path);
    iterator->second.mtime = time(nullptr);
    iterator->second.ctime = time(nullptr);

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
int MyInMemoryFS::fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    LOGF("--> Getting The List of Files of %s\n", path);

    LOG("Add the '.' and '..' entries");
    filler(buf, ".", NULL, 0); // Current Directory
    filler(buf, "..", NULL, 0); // Parent Directory

    // If the user is trying to show the files of the root directory show the following
    if (strcmp(path, "/") == 0) {
        // Add the names of the files in the directory
        for (const auto& iterator : files) {
            LOGF("Add '%s'", iterator.first.c_str());
            filler(buf, (iterator.first.c_str() + 1), NULL, 0);
        }
    }

    RETURN(0);
}

/// Initialize a file system.
///
/// This function is called when the file system is mounted. You may add some initializing code here.
/// \param [in] conn Can be ignored.
/// \return 0.
void* MyInMemoryFS::fuseInit(struct fuse_conn_info *conn) {
    // Open logfile
    this->logFile= fopen(((MyFsInfo *) fuse_get_context()->private_data)->logFile, "w+");
    if(this->logFile == NULL) {
        fprintf(stderr, "ERROR: Cannot open logfile %s\n", ((MyFsInfo *) fuse_get_context()->private_data)->logFile);
    } else {
        // turn of logfile buffering
        setvbuf(this->logFile, NULL, _IOLBF, 0);

        LOG("Starting logging...\n");

        LOG("Using in-memory mode");
    }

    files = {};  // Initialize files
    openFiles = {};  // Initialize openFiles

    RETURN(0);
}

/// @brief Clean up a file system.
///
/// This function is called when the file system is unmounted. You may add some cleanup code here.
void MyInMemoryFS::fuseDestroy() {
    LOGM();

    LOG("Freeing memory...");

    files.clear();
    openFiles.clear();

    LOG("Shutting down");
}

// DO NOT EDIT ANYTHING BELOW THIS LINE!!!

/// @brief Set the static instance of the file system.
///
/// Do not edit this method!
void MyInMemoryFS::SetInstance() {
    MyFS::_instance= new MyInMemoryFS();
}

