/*
 * file:        homework.c
 * description: skeleton file for CS 5600 homework 4
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * bigbug, updated December 2014
 */

#define FUSE_USE_VERSION 27

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <dirent.h>
#include <fuse.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include "cs5600fs.h"
#include "blkdev.h"

extern char *get_cwd();

#define DEBUG_MODE 0
#if DEBUG_MODE
    #define log(format, args...) \
    { \
        fprintf(stderr, format, ##args); \
    }   
#else
    #define log(format, args...)
#endif

#define DEBUG_FUSE 1

#define FE_PER_BLK  (FS_BLOCK_SIZE / sizeof(struct cs5600fs_entry))

// Convert from block unit to sector unit
#define SU(blk_idx)   ((blk_idx) << 1)

typedef enum {
    SEARCH_DIR,
    SEARCH_FILE, 
    SEARCH_BOTH
} search_type;

// The super block from the image
struct cs5600fs_super super;

// The fat table after the super block.
// We use stack vars so no memory management needs to be worried about.
// Its size is more than enough for this problem.
struct cs5600fs_entry fat[1024];

// The buffer to hold the path after adjusting
char final_path[128];

/* 
 * disk access - the global variable 'disk' points to a blkdev
 * structure which has been initialized to access the image file.
 *
 * NOTE - blkdev access is in terms of 512-byte SECTORS, while the
 * file system uses 1024-byte BLOCKS. Remember to multiply everything
 * by 2.
 */

extern struct blkdev *disk;

/* find the next word starting at 's', delimited by characters
 * in the string 'delim', and store up to 'len' bytes into *buf
 * returns pointer to immediately after the word, or NULL if done.
 */
static char *strwrd(char *s, char *buf, size_t len, char *delim)
{
    s += strspn(s, delim);
    int n = strcspn(s, delim);  /* count the span (spn) of bytes in */
    if (len - 1 < n)              /* the complement (c) of *delim */
        n = len - 1;
    memcpy(buf, s, n);
    buf[n] = 0;
    s += n;
    return *s == 0 ? NULL : s;
}

/* adjust_path - adjust the input path and return a processed one
 * which is good and easy to deal with.
 */ 
static char *adjust_path(const char *path) 
{    
    log("[origin] path: %s\n", path);
    char *new_path = final_path;
    memset(new_path, 0, sizeof(final_path));
#ifndef DEBUG_FUSE
    char search_path[128] = {0};
    char *p = search_path; // give it a shorter name 
    char *cwd = get_cwd();    

    // copy the input path because it cannot be changed
    strcpy(p, path);

    // get user printed path (p)
    p += strlen(cwd);
    if (*p == '/') ++p; 
    // log("p: %s\n", p);

    // get rid of repeated '/' at the beginning of p if possible
    int i = 0;
    while (p[i] == '/') ++i;
    if (i > 0) {
        p += i - 1; assert(*p == '/');
    }

    // get rid of any '/' at the tail
    if (strcmp(p, "/")) {
        i = strlen(p) - 1;
        while (p[i] == '/') --i;
        p[i + 1] = '\0';
    }

    // log("[adjust_path] cwd: %s, user_printed: %s\n", cwd, p);

    // make the new path    
    if (*p == '/') {       
        strcpy(new_path, p); // list from root 
    } else {
        // append the relative path to cwd, p[0] != '/'
        strcpy(new_path, cwd);
        if (strlen(p) > 0) {
            if (strcmp(cwd, "/"))
                strcat(new_path, "/");
            strcat(new_path, p);
        }   
    }
    
#else
    strcpy(new_path, path);
#endif
    log("[new path] new: %s\n", new_path);

    return new_path;
}

/* find_entry_by_name - For loop the given dir entries and find the first entry
 * whose name is the same as the input name. The entry index is returned.
 */
static int find_entry_by_name(struct cs5600fs_dirent dir[], const char *name) 
{
    int i = 0;
    for (; i < 16; ++i) {
        if (dir[i].valid && !strcmp(dir[i].name, name)) {
            break;
        }
    }    
    return i;
}

/* fs_find - find the block index from which the dirent corresponding
 * to the input dir path can be fetched. Also output the dirent of this file
 * and its containing dirent if containing is non-NULL.
 */
static int search(search_type type, const char *path, struct cs5600fs_dirent *ent, 
        struct cs5600fs_dirent *container)
{
    char name[44];
    struct cs5600fs_dirent dir[16];
    int i = -1, blk = super.root_dirent.start, keep = 1;
    memset(dir, 0, FS_BLOCK_SIZE);

    if (!strcmp(path, "/")) {
        *ent = super.root_dirent;
        if (type == SEARCH_DIR)
            return ent->start;    
        else if (type == SEARCH_FILE)
            return -EISDIR;
        else // BOTH
            return ent->start;
    }

    // init the first container if possible
    if (container) {
        *container = super.root_dirent;
    }

    while (keep) {
        if (i >= 0 && container) {
            // the container should point to the block which contains the output ent
            *container = dir[i];
            log("[find_dir] container name: %s\n", container->name);
        }
        path = strwrd((char *) path, name, sizeof(name), "/\0");
        disk->ops->read(disk, SU(blk), SU(1), (void *) dir); // read the block where this name is  
        // try to find the name in this directory
        for (i = 0; i < 16; ++i) {
            if (dir[i].valid && !strcmp(dir[i].name, name)) {
                if (type == SEARCH_DIR) { 
                    // the last of input path must be a dir name
                    blk  = dir[i].isDir ? dir[i].start : -ENOTDIR;
                    keep = dir[i].isDir;                      
                } else if (type == SEARCH_FILE) { 
                    // the last of input path must be a file name
                    blk  = dir[i].isDir ? (path ? dir[i].start : -EISDIR) : (path ? -ENOENT : dir[i].start);
                    keep = dir[i].isDir ? !!path : 0;                    
                } else { // BOTH
                    if (dir[i].isDir) {
                        blk  = dir[i].start;
                        keep = !!path;
                    } else {
                        blk  = path ? -ENOENT : dir[i].start;
                        keep = 0;
                    }
                }
                break; // found, so break
            }    
        }
        
        if (i >= 16) { // not found, end traversing
            blk  = -ENOENT; 
            keep = 0;
        } else {
            keep = !!path;
        }
    }

    if (blk > super.root_dirent.start)
        *ent = dir[i];
   
    return blk;
}

/* init - this is called once by the FUSE framework at startup.
 * This might be a good place to read in the super-block and set up
 * any global variables you need. You don't need to worry about the
 * argument or the return value.
 */
void* hw3_init(struct fuse_conn_info *conn)
{
    char buf[FS_BLOCK_SIZE];

    disk->ops->read(disk, SU(0), SU(1), buf);    
    memcpy(&super, buf, sizeof(super));

    log("Superblock:\n");
    log(" magic:                     %x\n", super.magic);
    log(" block size:                %d\n", super.blk_size);
    log(" file system size (blocks): %d\n", super.fs_size);
    log(" FAT length (blocks):       %d\n", super.fat_len);
    log(" Root directory start:      %d\n", super.root_dirent.start);

    disk->ops->read(disk, SU(1), SU(super.fat_len), (void *) fat);

    return NULL;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in CS5600fs are:
 *    st_nlink - always set to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */
static int hw3_getattr(const char *path, struct stat *sb)
{
    log("[hw3_getattr] get attr of %s\n", path);
    struct cs5600fs_dirent ent;
    int ret = search(SEARCH_BOTH, adjust_path(path), &ent, NULL);        
    if (ret < 0) return ret;

    memset(sb, 0, sizeof(struct stat));
    sb->st_uid    = ent.uid;
    sb->st_gid    = ent.gid;
    sb->st_mode   = ent.mode | (ent.isDir ? S_IFDIR : S_IFREG);
    sb->st_size   = ent.length;
    sb->st_mtime  = ent.mtime;
    sb->st_atime  = ent.mtime;
    sb->st_ctime  = ent.mtime;
    sb->st_nlink  = 1;
    sb->st_blocks = (sb->st_size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;

    return 0;
}

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */
static int hw3_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    struct stat sb;
    struct cs5600fs_dirent ent;
    struct cs5600fs_dirent dir[16];
    log("[hw3_readdir] read from dir %s\n", path);

    // locate the dir from the path
    int i, ret = search(SEARCH_DIR, adjust_path(path), &ent, NULL);        
    if (ret < 0) return ret;

    // read the block
    disk->ops->read(disk, SU(ent.start), SU(1), (void *) dir);
    
    for (i = 0; i < 16; ++i) {
        if (dir[i].valid) {
            memset(&sb, 0, sizeof(struct stat));
            sb.st_uid    = dir[i].uid;
            sb.st_gid    = dir[i].gid;
        	sb.st_mode   = dir[i].mode | (dir[i].isDir ? S_IFDIR : S_IFREG);
        	sb.st_size   = dir[i].length;
            sb.st_mtime  = dir[i].mtime;
        	sb.st_atime  = dir[i].mtime;
            sb.st_ctime  = dir[i].mtime;
            sb.st_nlink  = 1;
            sb.st_blocks = (sb.st_size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
        	filler(buf, dir[i].name, &sb, 0); /* invoke callback function */
        }
    }
    return 0;
}

/* create - create a new file with permissions (mode & 01777)
 *
 * Errors - path resolution, EEXIST
 *
 * If a file or directory of this name already exists, return -EEXIST.
 */
static int hw3_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi)
{
    struct cs5600fs_dirent ent;    
    struct cs5600fs_dirent dir[16];
    char *new_path = adjust_path(path);
    char dirpath[128] = {0}, filename[128] = {0};
    int i, j, ret;

    log("[hw3_create] %s\n", new_path);

    // try to get the filename and the containing dir path before it
    char *p = strrchr(new_path, '/') + 1;
    int offset = (p - 1 == new_path) ? 0 : -1;
    strcpy(filename, p);
    strncpy(dirpath, new_path, strlen(new_path) - strlen(p + offset));

    // I prefer to have it even the problem doesn't need this return value.
    if (strlen(filename) == 0) 
        return -EISDIR;

    log("[hw3_create] create %s in %s\n", filename, dirpath);

    // check whether a file or directory of this name aready exists
    if (search(SEARCH_BOTH, new_path, &ent, 0) > 0)
        return -EEXIST;

    // get the containing dir
    ret = search(SEARCH_DIR, dirpath, &ent, NULL);
    if (ret < 0) return ret;

    // read the block
    disk->ops->read(disk, SU(ent.start), SU(1), (void *) dir);

    // look for an invalid dirent so we can use for this file
    for (i = 0; i < 16; ++i)
        if (!dir[i].valid) break;
    if (i >= 16) 
        return -ENOSPC;

    // look for a block not in use by searching the fat
    for (j = super.root_dirent.start + 1; j < super.fs_size; ++j)
        if (!fat[j].inUse) break;
    if (j >= super.fs_size)
        return -ENOSPC;

    // init the fat entry for this new file block
    fat[j].inUse = 1;
    fat[j].eof   = 1;
    fat[j].next  = 0;

    // init the dir entry for this new file
    dir[i].valid  = 1;
    dir[i].isDir  = 0;
    dir[i].uid    = ent.uid;
    dir[i].gid    = ent.gid;
    dir[i].mode   = mode;
    dir[i].mtime  = time(NULL);
    dir[i].start  = j;
    dir[i].length = 0;
    strcpy(dir[i].name, filename);  // assume the given name length < 44

    // sync the memory data to the disk
    int fat_blk = j / FE_PER_BLK;
    disk->ops->write(disk, SU(fat_blk + 1), SU(1), (void *) &fat[fat_blk * FE_PER_BLK]);
    disk->ops->write(disk, SU(ent.start), SU(1), (void *) dir);

    return 0;
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 */ 
static int hw3_mkdir(const char *path, mode_t mode)
{
    struct cs5600fs_dirent ent;    
    struct cs5600fs_dirent dir[16];
    char *new_path = adjust_path(path);
    char dirpath[128] = {0}, dirname[128] = {0};
    int i, j, ret;

    log("[hw3_mkdir] %s\n", new_path);

    // try to get the dirname and the containing dir path before it
    char *p = strrchr(new_path, '/') + 1;
    int offset = (p - 1 == new_path) ? 0 : -1;
    strcpy(dirname, p);
    strncpy(dirpath, new_path, strlen(new_path) - strlen(p + offset));

    log("[hw3_mkdir] create %s in %s\n", dirname, dirpath);  

    // check whether a file or directory of this name aready exists
    if (search(SEARCH_BOTH, new_path, &ent, 0) > 0)
        return -EEXIST;
    
    // get the containing dir
    ret = search(SEARCH_DIR, dirpath, &ent, NULL);
    if (ret < 0) return ret;

    // read the block
    disk->ops->read(disk, SU(ent.start), SU(1), (void *) dir);

    // look for a dirent not been used for the new file
    for (i = 0; i < 16; ++i)
        if (!dir[i].valid) break;
    if (i >= 16) 
        return -ENOSPC;

    // look for a block not been used by searching the fat table
    for (j = super.root_dirent.start + 1; j < super.fs_size; ++j)
        if (!fat[j].inUse) break;
    if (j >= super.fs_size)
        return -ENOSPC;

    // init the fat entry for this new file block
    fat[j].inUse = 1;
    fat[j].eof   = 1;
    fat[j].next  = 0;

    // init the dir entry for this new file
    dir[i].valid  = 1;
    dir[i].isDir  = 1;
    dir[i].uid    = ent.uid;
    dir[i].gid    = ent.gid;
    dir[i].mode   = mode;
    dir[i].mtime  = time(NULL);
    dir[i].start  = j;
    dir[i].length = 0;
    strcpy(dir[i].name, dirname);  // assume the given name length < 44

    // sync the memory data to the disk
    int fat_blk = j / FE_PER_BLK;
    disk->ops->write(disk, SU(fat_blk + 1), SU(1), (void *) &fat[FE_PER_BLK * fat_blk]);
    disk->ops->write(disk, SU(ent.start), SU(1), (void *) dir);
    memset(dir, 0, sizeof(dir));
    disk->ops->write(disk, SU(j), SU(1), (void *) dir);

    return 0;
}

/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 */
static int hw3_unlink(const char *path)
{
    struct cs5600fs_dirent ent;
    struct cs5600fs_dirent dir[16];
    struct cs5600fs_dirent container;
    char *new_path = adjust_path(path);
    int i, j, ret;

    log("[hw3_unlink] %s\n", new_path);

    // check whether a file of this name aready exists
    ret = search(SEARCH_FILE, new_path, &ent, &container);
    if (ret < 0) return ret;

    // clear all fat entries of this file from ent.start
    i = ent.start;
    while (!fat[i].eof) {
        j = fat[i].next;
        memset(fat + i, 0, sizeof(struct cs5600fs_entry));
        i = j;
    }
    memset(fat + i, 0, sizeof(struct cs5600fs_entry));

    // modify the entry in the container dir
    disk->ops->read(disk, SU(container.start), SU(1), (void *) dir);    
    i = find_entry_by_name(dir, ent.name);
    memset(dir + i, 0, sizeof(struct cs5600fs_dirent));

    // sync the memory data to the disk
    disk->ops->write(disk, SU(container.start), SU(1), (void *) dir);
    disk->ops->write(disk, SU(1), SU(super.fat_len), (void *) fat);

    return 0;
}

/* rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
static int hw3_rmdir(const char *path)
{
    struct cs5600fs_dirent ent;    
    struct cs5600fs_dirent dir[16];
    struct cs5600fs_dirent container;
    char *new_path = adjust_path(path);
    int i, ret;

    // check whether a directory of this name aready exists
    ret = search(SEARCH_DIR, new_path, &ent, &container);
    if (ret < 0) return ret;

    // read the block
    disk->ops->read(disk, SU(ent.start), SU(1), (void *) dir);    
    log("[hw3_rmdir] ent: %s %d\n", ent.name, ent.start);

    // look for any valid dirent that prevents the dir from being removed
    for (i = 0; i < 16; ++i) {
        if (dir[i].valid) {
            return -ENOTEMPTY;
        }
    }
    
    // clear the fat entry and dirent of the directory in memory
    memset(fat + ent.start, 0, sizeof(fat[0]));
    disk->ops->read(disk, SU(container.start), SU(1), (void *) dir);
    for (i = 0; i < 16; ++i) {
        if (dir[i].valid && !strcmp(dir[i].name, ent.name)) {
            memset(dir + i, 0, sizeof(dir[0]));
            break;
        }
    }

    log("[hw3_rmdir] container: %s %d\n", container.name, container.start);    

    // sync the memory data to the disk
    int fat_blk = ent.start / FE_PER_BLK;
    disk->ops->write(disk, SU(fat_blk + 1), SU(1), (void *) &fat[fat_blk * FE_PER_BLK]);
    disk->ops->write(disk, SU(container.start), SU(1), (void *) dir);

    return 0;
}

/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
static int hw3_rename(const char *src_path, const char *dst_path)
{
    struct cs5600fs_dirent ent[2];
    struct cs5600fs_dirent dir[16];
    struct cs5600fs_dirent container[2];
    char new_src_path[128] = {0}, new_dst_path[128] = {0};
    char *tmp;
    int i, ret;

    // adjust paths
    tmp = adjust_path(src_path);
    strcpy(new_src_path, tmp);
    tmp = adjust_path(dst_path);
    strcpy(new_dst_path, tmp);

    log("[hw3_rename] %s to %s\n", new_src_path, new_dst_path);

    // check source does not exist
    ret = search(SEARCH_BOTH, src_path, &ent[0], &container[0]);
    if (ret < 0) return ret;

    // check destination already exists
    ret = search(SEARCH_BOTH, dst_path, &ent[1], &container[1]);
    if (ret > 0) return -EEXIST;

    // ent[0] is valid while ent[1] is not, both containers are valid
    // check source and destination are not in the same directory
    if (container[0].start != container[1].start) return -EINVAL;

    // modify the entry in the container dir
    disk->ops->read(disk, SU(container[0].start), SU(1), (void *) dir);
    i = find_entry_by_name(dir, ent[0].name);

    // get the name from new_dst_path
    char *new_name = strrchr(new_dst_path, '/') + 1;
    assert(strlen(new_name) > 0);
    strcpy(dir[i].name, new_name);

    // sync the memory data to the disk
    disk->ops->write(disk, SU(container[0].start), SU(1), (void *) dir);

    return 0;
}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 */
static int hw3_chmod(const char *path, mode_t mode)
{
    struct cs5600fs_dirent ent;
    struct cs5600fs_dirent dir[16];
    struct cs5600fs_dirent container;
    char *new_path = adjust_path(path);
    int i, ret;

    log("[hw3_chmod] %s\n", new_path);

    // check whether a file of this name aready exists
    ret = search(SEARCH_FILE, new_path, &ent, &container);
    if (ret < 0) return ret;

    // modify the entry in the container dir
    disk->ops->read(disk, SU(container.start), SU(1), (void *) dir);    
    i = find_entry_by_name(dir, ent.name);
    dir[i].mode = mode;

    // sync the memory data to the disk
    disk->ops->write(disk, SU(container.start), SU(1), (void *) dir);

    return 0;
}

int hw3_utime(const char *path, struct utimbuf *ut)
{
    struct cs5600fs_dirent ent;
    struct cs5600fs_dirent dir[16];
    struct cs5600fs_dirent container;
    char *new_path = adjust_path(path);
    int i, ret;

    log("[hw3_utime] %s\n", new_path);

    // check whether a file of this name aready exists
    ret = search(SEARCH_FILE, new_path, &ent, &container);
    if (ret < 0) return ret;

    // modify the entry in the container dir
    disk->ops->read(disk, SU(container.start), SU(1), (void *) dir);
    i = find_entry_by_name(dir, ent.name);
    dir[i].mtime = time(NULL);

    // sync the memory data to the disk
    disk->ops->write(disk, SU(container.start), SU(1), (void *) dir);

    return 0;
}

/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int hw3_truncate(const char *path, off_t len)
{   
    struct cs5600fs_dirent ent;
    struct cs5600fs_dirent dir[16];
    struct cs5600fs_dirent container;
    char *new_path = adjust_path(path);
    int i, j, eof, ret;

    log("[hw3_truncate] %s\n", new_path);

    if (len != 0)
       return -EINVAL;      /* invalid argument */

    // check whether a file of this name aready exists
    ret = search(SEARCH_FILE, new_path, &ent, &container);
    if (ret < 0) return ret;

    // clear all fat entries of this file from ent.start except the first 
    i = fat[ent.start].next; eof = fat[ent.start].eof;
    while (!eof) {
        eof = fat[i].eof;
        j = fat[i].next;
        memset(fat + i, 0, sizeof(struct cs5600fs_entry));
        i = j;
    }    
    fat[ent.start].eof  = 1;
    fat[ent.start].next = 0;
    assert(fat[ent.start].inUse);

    // modify the entry in the container dir
    disk->ops->read(disk, SU(container.start), SU(1), (void *) dir);    
    i = find_entry_by_name(dir, ent.name);
    dir[i].length = 0;

    // sync the memory data to the disk
    disk->ops->write(disk, SU(1), SU(super.fat_len), (void *) fat);
    disk->ops->write(disk, SU(container.start), SU(1), (void *) dir);

    return 0;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - you hit the end of the file, return < len
 *   - if offset is beyond the end of the file, return 0
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
static int hw3_read(const char *path, char *buf, size_t len, off_t offset,
		    struct fuse_file_info *fi)
{
    // assume buf size always equal or larger than len
    char cache[FS_BLOCK_SIZE];
    struct cs5600fs_dirent ent;

    assert(len > 0);
    log("[hw3_read] read len: %d\n", len);

    // locate the file from the path
    int ret = search(SEARCH_FILE, adjust_path(path), &ent, NULL);
    if (ret < 0) return ret;
    assert(fat[ret].inUse);

    // if offset is beyond the end of the file
    if (offset >= ent.length) 
        return 0; 

    // will hit the end of the file
    if (len > ent.length - offset) 
        len = ent.length - offset;

    // find the starting block from the offset
    int i, start = ent.start, offset_in_blk = offset % FS_BLOCK_SIZE;
    for (i = 0; i < offset / FS_BLOCK_SIZE; ++i) {
        assert(!fat[start].eof);
        start = fat[start].next;
        assert(fat[start].inUse);
        // log("[hw3_read] %d\n", start);        
    }

    // read data from the starting block
    int read = len, fragment = FS_BLOCK_SIZE - offset_in_blk;
    disk->ops->read(disk, SU(start), SU(1), cache);
    if (read >= fragment) {
        memcpy(buf, cache + offset_in_blk, fragment);
        buf  += fragment;
        read -= fragment;
    } else {
        memcpy(buf, cache + offset_in_blk, read);
        read = 0;
    }

    while (read >= FS_BLOCK_SIZE) {
        start = fat[start].next; assert(fat[start].inUse);
        disk->ops->read(disk, SU(start), SU(1), cache);
        memcpy(buf, cache, FS_BLOCK_SIZE);
        buf  += FS_BLOCK_SIZE;
        read -= FS_BLOCK_SIZE;
    }

    if (read > 0) {
        start = fat[start].next; assert(fat[start].inUse);
        disk->ops->read(disk, SU(start), SU(1), cache);
        memcpy(buf, cache, read);        
    }

    return len;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 */
static int hw3_write(const char *path, const char *buf, size_t len,
		     off_t offset, struct fuse_file_info *fi)
{
    // assume buf size always equal or larger than len
    char cache[FS_BLOCK_SIZE];
    struct cs5600fs_dirent ent;
    struct cs5600fs_dirent dir[16];
    struct cs5600fs_dirent container;
    int i, j, ret;

    assert(len > 0);

    // locate the file from the path
    ret = search(SEARCH_FILE, adjust_path(path), &ent, &container);
    if (ret < 0) return ret;
    assert(fat[ret].inUse);

    // if 'offset' is greater than current file length
    if (offset > ent.length)
        return -EINVAL;

    log("[hw3_write] input len: %d\n", len);
    log("[hw3_write] current file len: %d\n", ent.length);

    // compute the left available space in the last file block
    int avail = (ent.length % FS_BLOCK_SIZE == 0) ? 0 : FS_BLOCK_SIZE - ent.length % FS_BLOCK_SIZE;  
    if (ent.length == 0) {
        avail = FS_BLOCK_SIZE;
    }
    log("[hw3_write] avail size: %d\n", avail);

    // compute the size of bytes that needs to allocate more blocks
    int more_space = len - avail;
    int more_fat = 0;
    if (more_space > 0) {
        more_fat = more_space / FS_BLOCK_SIZE;
        more_fat += more_space % FS_BLOCK_SIZE == 0 ? 0 : 1;
    }
    log("[hw3_write] alloc_more_fat: %d\n", more_fat);

    // we need more blocks so more fat entries have to be used
    int next = ent.start; assert(fat[next].inUse);
    if (more_fat > 0) {
        while (!fat[next].eof) 
            next = fat[next].next;
        fat[next].eof = 0;
        log("[hw3_write] last fat entry before alloc: %d\n", next);
        // now 'next' points to the last fat entry of this file,
        // we can check the fat table sequentially to find entries not in use
        j = super.root_dirent.start + 1;
        for (i = 0; i < more_fat; ++i) {
            // log("[hw3_write]")
            for (; j < super.fs_size; ++j)
                if (!fat[j].inUse) {
                    break;
                }
            if (j >= super.fs_size)
                return -ENOSPC;

            // update the last fat entry
            fat[next].next = j; // link to a new fat entry
            fat[j].inUse = 1;
            fat[j].eof   = 0;            
            next = j;
        }
        fat[next].inUse = 1;
        fat[next].eof   = 1;
        fat[next].next  = 0;

        // sync the entire fat table for simplicity
        disk->ops->write(disk, SU(1), SU(super.fat_len), (void *) fat);
    }

    // find the start block index based on the input offset      
    int write = len;
    int start = ent.start;
    int blk_offset = offset % FS_BLOCK_SIZE;
    for (i = 0; i < offset / FS_BLOCK_SIZE; ++i) {
        start = fat[start].next;
    }
    log("[hw3_write] start_block: %d\n", start);
    log("[hw3_write] blk_offset : %d\n", blk_offset);

    // head part
    disk->ops->read(disk, SU(start), SU(1), cache);
    if (write >= FS_BLOCK_SIZE - blk_offset) {
        memcpy(cache + blk_offset, buf, FS_BLOCK_SIZE - blk_offset);
        buf   += FS_BLOCK_SIZE - blk_offset;
        write -= FS_BLOCK_SIZE - blk_offset;
    } else {
        memcpy(cache + blk_offset, buf, write);
        write = 0;
    }
    disk->ops->write(disk, SU(start), SU(1), cache);    

    // body part
    while (write >= FS_BLOCK_SIZE) {
        start = fat[start].next; assert(fat[start].inUse);
        log("[hw3_write] middle block: %d\n", start);
        disk->ops->write(disk, SU(start), SU(1), cache);
        buf   += FS_BLOCK_SIZE;
        write -= FS_BLOCK_SIZE;
    }       

    // tail part
    if (write > 0) {        
        start = fat[start].next; assert(fat[start].inUse);
        log("[hw3_write] last block: %d\n", start);
        disk->ops->read(disk, SU(start), SU(1), cache);
        memcpy(cache, buf, write);
        disk->ops->write(disk, SU(start), SU(1), cache);
    }    

    // modify the entry in the container dir and write it back
    disk->ops->read(disk, SU(container.start), SU(1), (void *) dir);    
    i = find_entry_by_name(dir, ent.name);
    dir[i].length += len;
    disk->ops->write(disk, SU(container.start), SU(1), (void *) dir);

    return len;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
static int hw3_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superblock + FAT)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */
    int i, used = 0, fat_table_size = super.fat_len * FE_PER_BLK;
    for (i = 1 + super.fat_len; i < fat_table_size; ++i) {
        used += fat[i].inUse;
    }
    memset(st, 0, sizeof(struct statvfs));
    st->f_bsize   = FS_BLOCK_SIZE;
    st->f_blocks  = super.fs_size - (1 + super.fat_len);
    st->f_bfree   = st->f_blocks - used;
    st->f_bavail  = st->f_bfree;
    st->f_namemax = sizeof(super.root_dirent.name) - 1;
    return 0;
}

/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'hw3_ops'.
 */
struct fuse_operations hw3_ops = {
    .init = hw3_init,
    .getattr = hw3_getattr,
    .readdir = hw3_readdir,
    .create = hw3_create,
    .mkdir = hw3_mkdir,
    .unlink = hw3_unlink,
    .rmdir = hw3_rmdir,
    .rename = hw3_rename,
    .chmod = hw3_chmod,
    .utime = hw3_utime,
    .truncate = hw3_truncate,
    .read = hw3_read,
    .write = hw3_write,
    .statfs = hw3_statfs,
};

