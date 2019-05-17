#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <sys/stat.h>

#include "ext2.h"

#define BASE_OFFSET 1024
#define EXT2_BLOCK_SIZE 1024
#define IMAGE "image.img"

typedef unsigned char bmap;
#define __NBITS (8 * (int) sizeof (bmap))
#define __BMELT(d) ((d) / __NBITS)
#define __BMMASK(d) ((bmap) 1 << ((d) % __NBITS))
#define BM_SET(d, set) ((set[__BMELT (d)] |= __BMMASK (d)))
#define BM_CLR(d, set) ((set[__BMELT (d)] &= ~__BMMASK (d)))
#define BM_ISSET(d, set) ((set[__BMELT (d)] & __BMMASK (d)) != 0)

unsigned int block_size = 0;
#define BLOCK_OFFSET(block) (block*block_size)

/* Function Prototypes */
int resolveTargetInode(char *);
void fillMeta(ext2_inode*, int);
void changeBlockGroup(int, bool);
unsigned int writeToBlock(unsigned char *);
size_t actualDirEntrySize(unsigned int);
size_t actualDirEntrySize(struct ext2_dir_entry*);
void allocateNewInode();
void putToTarget(char *);

/* Global Variables */
struct ext2_inode newInode;
int inodeNo = 2, blockNo, inodeIndex, containingBgId, sFileSize;
struct ext2_super_block super;
struct ext2_group_desc group;
bmap* blockBitmap;
bmap* inodeBitmap;
int image, sFile, currGid, numGroups, newInodeGid, targetInodeNo;

int main(int argc, char* argv[]) {

    /* Open ext2 image file */
    if ((image = open(argv[1], O_RDWR)) < 0) {
        fprintf(stderr, "Could not open img file RDWR\n");
        return 1;
    }

    /* Read superblock into super */
    lseek(image, BASE_OFFSET, SEEK_SET);
    read(image, &super, sizeof(super));
    if (super.s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Img file could not be classified as a ext2 fs\n");
        return 1;
    }
    block_size = 1 << (10 + super.s_log_block_size);

    blockBitmap = new bmap[block_size];
    inodeBitmap = new bmap[block_size];

    /* Read Zeroth Group descriptor into group and read Bitmaps accordingly with BG */
    numGroups = (super.s_inodes_per_group + super.s_inodes_count - 1)/super.s_inodes_per_group;
    changeBlockGroup(0, true);

    /* Read target */
    targetInodeNo = resolveTargetInode(argv[3]);

    /* Open sourcefile */
    if ((sFile = open(argv[2], O_RDONLY)) < 0) {
        fprintf(stderr, "Could not open source file RDONLY\n");
        return 2;
    }

    /* Get fileName */
    char *fileName;
    char temp[EXT2_NAME_LEN];
    strncpy(temp, argv[2], EXT2_NAME_LEN);
    char *ptr = strtok(temp, "/");
    while(ptr) {
        fileName = ptr;
        ptr = strtok(NULL, "/");
    }

    /* Handle newInode stuff for your new copy of sourcefile */
    changeBlockGroup(0, false);
    allocateNewInode();
    fillMeta(&newInode, sFile);
    std::cout << inodeNo << " ";

    /* Copy sourcefile contents to new data blocks for new inode */
    unsigned char block[block_size];
    unsigned int size = 0, directCounter = 0, readBytes;
    blockNo = super.s_first_data_block;
    changeBlockGroup(0, false);
    lseek(sFile, (off_t) 0, SEEK_SET);
    while (size <= sFileSize && directCounter < 12) {
        /* Read block_size many bytes from sFile to block and write it to First non-reserved data block */
        readBytes = read(sFile, block, block_size);
        if (readBytes < block_size)
            memset(block + readBytes, 0, block_size - readBytes);
        newInode.i_block[directCounter] = writeToBlock(block);
        std::cout << blockNo << " ";
        size += block_size;
        directCounter++;
    }

    newInode.i_blocks = directCounter * (block_size / 512);

    putToTarget(fileName);

    /* Write all changes to image */

    /* Overwrite bitmaps into image */
    lseek(image, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
    write(image, inodeBitmap, block_size);

    lseek(image, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
    write(image, blockBitmap, block_size);

    /* Overwrite changes in group and super */
    lseek(image, BASE_OFFSET+sizeof(super)+sizeof(group)*currGid, SEEK_SET);
    write(image, &group, sizeof(group));

    lseek(image, BASE_OFFSET, SEEK_SET);
    write(image, &super, sizeof(super));

    /* Write inode to img */
    changeBlockGroup(newInodeGid, false);
    inodeIndex = (inodeNo - 1) % super.s_inodes_per_group;
    lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (inodeIndex * sizeof(struct ext2_inode)), SEEK_SET);
    write(image, &newInode, sizeof(struct ext2_inode));

    /* Free heap memory */
    delete [] blockBitmap;
    delete [] inodeBitmap;

    /* Close sourcefile */
    close(sFile);

    /* Close ext2 image file*/
    close(image);

    std::cout << "\n";
    return 0;
}

int resolveTargetInode(char * target) {
    int num = atoi(target);
    if (num)
        return num;
    else if (!strncmp(target, "/", EXT2_NAME_LEN))
        return 2;
    else {
        /* Construct a vector of paths */
        std::vector<char*> path;
        char temp[255];
        strncpy(temp, target, 255);
        char *ptr = strtok(temp, "/");
        while(ptr) {
            path.push_back(ptr);
            ptr = strtok(NULL, "/");
        }

        /* Starting from root inode(2) traverse the path and find target inode */
        bool found = false, innerFlag;
        unsigned char block[block_size];
        int tempInodeNo = 2, pathIndex = 0;
        struct ext2_inode tempInode;

        while (!found) {
            // Read tempInodeNo's inode into tempInode
            inodeIndex = (tempInodeNo - 1) % super.s_inodes_per_group;
            lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (inodeIndex * sizeof(struct ext2_inode)), SEEK_SET);
            read(image, &tempInode, sizeof(struct ext2_inode));

            // Traverse tempInode's data blocks
            innerFlag = false;
            for (int i = 0; i < 12 ; ++i) {
                unsigned int size = 0;
                lseek(image, BLOCK_OFFSET(tempInode.i_block[i]), SEEK_SET);
                read(image, block, block_size);
                struct ext2_dir_entry* entry = (struct ext2_dir_entry*)block;

                // Traverse dir entries of data block i
                while (size < block_size) { // && entry->inode
                    if (!strncmp(entry->name, path[pathIndex], strlen(path[pathIndex]))) {
                        /* Found the entry in current dir hierarchy */
                        if (pathIndex+1 == path.size()) {
                            /* Found the inode of the specified target directory */
                            found = true;
                        }
                        tempInodeNo = entry->inode; // switch inode
                        innerFlag = true;
                        break;
                    }
                    /* move to the next entry */
                    size += entry->rec_len;
                    entry = (ext2_dir_entry*)((char*)entry + entry->rec_len);
                }
                if (innerFlag)
                    break;
            }
            pathIndex++;
            containingBgId = (tempInodeNo - 1) / super.s_inodes_per_group;
            changeBlockGroup(containingBgId, false);
        }
        return tempInodeNo;
    }
}

void fillMeta(ext2_inode * inode, int file) {

    struct stat fileStat;
    if(fstat(file, &fileStat) < 0) {
        fprintf(stderr, "Could not read stat of sourcefile, exitting.\n");
        exit(2);
    }
    inode->i_mode = fileStat.st_mode;
    inode->i_uid = fileStat.st_uid;
    inode->i_gid = fileStat.st_gid;
    inode->i_size = fileStat.st_size;
    inode->i_atime = fileStat.st_atime;
    inode->i_mtime = fileStat.st_mtime;
    inode->i_ctime = fileStat.st_ctime;
    inode->i_links_count = 1;
    sFileSize = fileStat.st_size;
}

void changeBlockGroup(int bgID, bool first) {

    if (bgID >= numGroups) {
        fprintf(stderr, "Memory limit reached, no more block groups!\n");
        exit(3);
    }
    else if (!first && bgID == currGid)
        return;

    if (!first) {
        /* First Overwrite changes in old Group and bitmaps*/
        lseek(image, BASE_OFFSET+sizeof(super)+sizeof(group)*currGid, SEEK_SET);
        write(image, &group, sizeof(group));

        lseek(image, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
        write(image, blockBitmap, block_size);

        lseek(image, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
        write(image, inodeBitmap, block_size);
    }

    currGid = bgID;

    /* Then change BG */
    lseek(image, BASE_OFFSET+sizeof(super)+sizeof(group)*currGid, SEEK_SET);
    read(image, &group, sizeof(group));

    /* and change bitmaps */
    lseek(image, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
    read(image, blockBitmap, block_size);

    lseek(image, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
    read(image, inodeBitmap, block_size);
}

void allocateNewInode() {
    /* Place new inode into next empty place for inodes */

    while (true) {
        inodeIndex = (inodeNo - 1) % super.s_inodes_per_group;
        if (!BM_ISSET(inodeIndex, inodeBitmap)) {
            containingBgId = (inodeNo - 1) / super.s_inodes_per_group;
            newInodeGid = containingBgId;
            BM_SET(inodeIndex, inodeBitmap);
            group.bg_free_inodes_count--;
            super.s_free_inodes_count--;
            lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (inodeIndex * sizeof(struct ext2_inode)), SEEK_SET);
            read(image, &newInode, sizeof(struct ext2_inode));
            break;
        }
        if (inodeNo && !(inodeNo % super.s_inodes_per_group)) {
            // Block group border reached, Switch group
            inodeNo++;
            containingBgId = (inodeNo - 1) / super.s_inodes_per_group;
            changeBlockGroup(containingBgId, false);
        }
        else
            inodeNo++;
    }
}

void putToTarget(char * fileName) {
    /* Insert a dirEntry in target data blocks for mapping of new inode to target */

    /* Go to targetInode and get its directory data blocks */
    containingBgId = (targetInodeNo - 1) / super.s_inodes_per_group;
    changeBlockGroup(containingBgId, false);

    // go to targetInode
    struct ext2_inode targetInode;
    unsigned int targetInodeIndex = (targetInodeNo - 1) % super.s_inodes_per_group, tGid = currGid;
    lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (targetInodeIndex * sizeof(struct ext2_inode)), SEEK_SET);
    read(image, &targetInode, sizeof(struct ext2_inode));

    const size_t newEntrySize = actualDirEntrySize(strlen(fileName));

    // Traverse targetInode's data blocks
    unsigned char buff[block_size];
    unsigned int size;
    bool found = false;
    for (int i = 0; i < 12 ; ++i) {

        if (!targetInode.i_block[i]) {
            /* if current data block of directory is unallocated allocate a new data block
             * for this dir with new entry in it and its rec_len set to block_size */
            memset(buff, 0, block_size);
            struct ext2_dir_entry* newLastEntry = (struct ext2_dir_entry*)((char*)buff);
            newLastEntry->inode = inodeNo;
            newLastEntry->file_type = EXT2_FT_REG_FILE;
            strncpy(newLastEntry->name, fileName, strlen(fileName));
            newLastEntry->name_len = strlen(fileName);
            newLastEntry->rec_len = block_size;

            targetInode.i_block[i] = writeToBlock(buff);
            changeBlockGroup(tGid, false);
            lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (targetInodeIndex * sizeof(struct ext2_inode)), SEEK_SET);
            write(image, &targetInode, sizeof(struct ext2_inode));
            break;
        }

        lseek(image, BLOCK_OFFSET(targetInode.i_block[i]), SEEK_SET);
        read(image, buff, block_size);
        // convert data block to entry
        struct ext2_dir_entry* entry = (struct ext2_dir_entry*) buff;

        // Traverse dir entries of data block i
        size = 0;
        while (size < block_size) {
            const size_t realEntrySize = actualDirEntrySize(entry);
            if (entry->rec_len > realEntrySize) {
                /* Last entry found */
                if (realEntrySize + newEntrySize <= entry->rec_len - realEntrySize) {
                    /* If new entry fits to this data block */
                    /* Found an empty entry, fill its data */
                    struct ext2_dir_entry* newLastEntry = (struct ext2_dir_entry*)((char*)entry + realEntrySize);
                    newLastEntry->inode = inodeNo;
                    newLastEntry->file_type = EXT2_FT_REG_FILE;
                    strncpy(newLastEntry->name, fileName, strlen(fileName));
                    newLastEntry->name_len = strlen(fileName);
                    newLastEntry->rec_len = entry->rec_len - realEntrySize;
                    entry->rec_len = realEntrySize;

                    lseek(image, BLOCK_OFFSET(targetInode.i_block[i]), SEEK_SET);
                    write(image, buff, block_size);
                    found = true;
                    break;
                }
            }

            /* move to the next entry */
            size += entry->rec_len;
            entry = (ext2_dir_entry*)((char*)entry + entry->rec_len);
        }

        if (found)
            break;
    }
}

unsigned int writeToBlock(unsigned char * block) {
    /* Finds new available data block and writes data in "block" to there */
    /* Returns data block id */
    unsigned int blockIndex;
    while (true) {
        if (blockNo) {
            if (block_size == 1024)
                blockIndex = (blockNo - 1) % super.s_blocks_per_group;
            else
                blockIndex = blockNo % super.s_blocks_per_group;
        }
        else
            blockIndex = 0;

        if (!BM_ISSET(blockIndex, blockBitmap)) {
            group.bg_free_blocks_count--;
            super.s_free_blocks_count--;
            BM_SET(blockIndex, blockBitmap);
            if (block_size == 1024)
                lseek(image, BLOCK_OFFSET(super.s_first_data_block) + (block_size * blockIndex), SEEK_SET);
            else
                lseek(image, BLOCK_OFFSET(blockIndex), SEEK_SET);
            write(image, block, block_size);
            break;
        }
        if (blockNo && !(blockNo % super.s_blocks_per_group)) {
            // Block group border reached, Switch group
            blockNo++;
            containingBgId = (blockNo - 1) / super.s_blocks_per_group;
            changeBlockGroup(containingBgId, false);
        }
        else
            blockNo++;
    }
    return blockNo;
}

size_t actualDirEntrySize(unsigned int name_len) {
    return (8 + name_len + 3) & (~3);
}

size_t actualDirEntrySize(struct ext2_dir_entry* entry) {
    return (8 + entry->name_len + 3) & (~3);
}