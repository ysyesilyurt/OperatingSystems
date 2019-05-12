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
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)

/* Function Prototypes */
int resolveTargetInode(char *);
void fillMeta(ext2_inode*, int);
void changeBlockGroup(int, bool);
unsigned int writeToBlock(unsigned char[]);
size_t actualDirEntrySize(unsigned int);
size_t actualDirEntrySize(struct ext2_dir_entry*);
void allocateNewInode();
void putToTarget(char *);

/* Global Variables */
struct ext2_inode newInode;
unsigned int inodeNo = 2, containingBgId, sFileSize;
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
    numGroups = super.s_inodes_count/super.s_inodes_per_group; // ??? ceiling of this division ?
    changeBlockGroup(0, true);

    /* Read target */
    targetInodeNo = resolveTargetInode(argv[3]);
    // std::cout << "targetInodeNo: " << targetInodeNo << "\n";

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

    /* Handle newInode stuff for your to-be-copied */
    allocateNewInode();
    fillMeta(&newInode, sFile);
    std::cout << inodeNo << " ";

    /* Copy sourcefile contents to new data blocks for new inode */
    unsigned char block[block_size];
    unsigned int size = 0, directCounter = 0, readBytes;
    lseek(sFile, (off_t) 0, SEEK_SET);
    while (size <= sFileSize && directCounter < 12) {
        /* Read block_size many bytes from sFile to block and write it to First non-reserved data block */
        readBytes = read(sFile, block, block_size);
        if (readBytes < block_size)
            memset(block + readBytes, 0, block_size - readBytes);
        newInode.i_block[directCounter] = writeToBlock(block);
        size += block_size;
        directCounter++;
    }

    newInode.i_blocks = directCounter + 1; // 512 BYTES ?!!!!!!!!!!!!??????????????!!!!!!!!!!!

    putToTarget(fileName);

    /* Write all changes to image */

    /* Write inode to img */
    // change group desc if needed
    changeBlockGroup(newInodeGid, false);
    lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (inodeNo - 1) * sizeof(struct ext2_inode), SEEK_SET);
    write(image, &newInode, sizeof(struct ext2_inode));

    /* Overwrite bitmaps into image */
    lseek(image, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
    write(image, inodeBitmap, block_size);

    lseek(image, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
    write(image, blockBitmap, block_size);

    /* Overwrite changes in group and super */
    lseek(image, BASE_OFFSET + block_size, SEEK_SET);
    write(image, &group, sizeof(group));

    lseek(image, BASE_OFFSET, SEEK_SET);
    write(image, &super, sizeof(super));

    /* Free heap memory */
    delete [] blockBitmap;
    delete [] inodeBitmap;

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
        unsigned int containingBgId;
        int tempInodeNo = 2, pathIndex = 0;
        ext2_inode tempInode;
        while (!found) {
            // Read tempInodeNo's inode into tempInode
            lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (tempInodeNo - 1) * sizeof(struct ext2_inode),
                  SEEK_SET);
            read(image, &tempInode, sizeof(struct ext2_inode));

            // Traverse tempInode's data blocks
            innerFlag = false;
            for (int i = 0; i < 12 ; ++i) {
                unsigned int size = 0;
                lseek(image, BLOCK_OFFSET(tempInode.i_block[i]), SEEK_SET);
                read(image, block, block_size);
                struct ext2_dir_entry* entry = (struct ext2_dir_entry*)block;

                // Traverse dir entries of data block i
                while (size <= block_size && entry->inode) { // lost_found_inode.i_size ?, if entry->inode => switch to a new data block
                    // Dont forget that each dir-entry in here corresponds to a file/dir under this directory
                    if (!strncmp(entry->name, path[pathIndex], EXT2_NAME_LEN)) {
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
            // change group desc if needed
            changeBlockGroup(containingBgId, false);
        }
        return tempInodeNo;
    }
}

void fillMeta(ext2_inode * inode, int file) {
    struct stat fileStat;
    if(fstat(file, &fileStat) < 0) {
        fprintf(stderr, "Could not read stat of sourcefile, exitting.");
        exit(2);
    }
    inode->i_mode = fileStat.st_mode;
    inode->i_uid = fileStat.st_uid;
    inode->i_gid = fileStat.st_gid;
    inode->i_size = fileStat.st_size;
    inode->i_blocks = fileStat.st_blocks; // ?? st_blocks gives number of 512B blocks allocated, we have a block size 1K
    inode->i_atime = fileStat.st_atime;
    inode->i_mtime = fileStat.st_mtime;
    inode->i_ctime = fileStat.st_ctime;
    inode->i_links_count = 1;
    sFileSize = fileStat.st_size;
}

void changeBlockGroup(int bgID, bool first) {

    if (bgID >= numGroups) {
        fprintf(stderr, "Memory limit reached, no more block groups!");
        exit(2);
    }

    if (!first) {
        /* First Overwrite changes in old Group and bitmaps*/
        lseek(image, BASE_OFFSET+sizeof(super)+sizeof(group)*currGid, SEEK_SET);
        write(image, &group, sizeof(group));

        lseek(image, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
        write(image, inodeBitmap, block_size);

        lseek(image, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
        write(image, blockBitmap, block_size);
    }

    currGid = bgID;

    /* Then change BG */
    lseek(image, BASE_OFFSET+sizeof(super)+sizeof(group)*bgID, SEEK_SET);
    read(image, &group, sizeof(group));

    /* and change bitmaps */
    lseek(image, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
    read(image, blockBitmap, block_size);

    lseek(image, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
    read(image, inodeBitmap, block_size);
}

void allocateNewInode() {
    /* Place new inode into next empty place for inodes */
    unsigned int size = 0;
    while (true) {
        if (!BM_ISSET(inodeNo-1, inodeBitmap)) {
            unsigned int containingBgId = (inodeNo - 1) / super.s_inodes_per_group;
            newInodeGid = containingBgId;
            // change group desc if needed
            changeBlockGroup(containingBgId, false);
            // increment next available inodeNo
            BM_SET(inodeNo-1, inodeBitmap);
            group.bg_free_inodes_count--;
            super.s_free_inodes_count--;
            lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (inodeNo - 1) * sizeof(struct ext2_inode), SEEK_SET);
            read(image, &newInode, sizeof(struct ext2_inode));
            break;
        }
        if (size + super.s_inode_size >= super.s_inodes_count * super.s_inode_size) {
            // Block group border reached, Change group
            unsigned int containingBgId = (inodeNo - 1) / super.s_inodes_per_group; // (blockNo - 1) ?
            changeBlockGroup(containingBgId, false);
            size = 0;
        }
        else {
            size += super.s_inode_size;
            inodeNo++;
        }
    }
}

void putToTarget(char * fileName) {
    /* Insert a dirEntry in target data blocks for mapping of new inode to target */

    /* Go to targetInode and get its directory data blocks */
    // change group desc if needed
    containingBgId = (targetInodeNo - 1) / super.s_inodes_per_group;
    changeBlockGroup(containingBgId, false);

    // go to targetInode
    struct ext2_inode targetInode;
    unsigned int targetInodeIndex = targetInodeNo % super.s_inodes_per_group;
    lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (targetInodeIndex - 1) * sizeof(struct ext2_inode), SEEK_SET);
    read(image, &targetInode, sizeof(struct ext2_inode));

    const size_t newEntrySize = actualDirEntrySize(strlen(fileName));

    // Traverse targetInode's data blocks
    unsigned char buff[block_size];
    unsigned int size;
    bool found = false;
    for (int i = 0; i < 12 ; ++i) {
        lseek(image, BLOCK_OFFSET(targetInode.i_block[i]), SEEK_SET);
        read(image, buff, block_size);
        // convert data block to entry
        struct ext2_dir_entry* entry = (struct ext2_dir_entry*) buff;

        // Traverse dir entries of data block i
        size = 0;
        while (size < block_size) { // entry->inode ???
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

                    /* Write changes to in dir entry to image */
                    if (!BM_ISSET(targetInode.i_block[i], blockBitmap)) // -1 ?
                        BM_SET(targetInode.i_block[i], blockBitmap); // -1 ?
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

unsigned int writeToBlock(unsigned char block[]) {
    /* Finds new available data block and writes data in "block" to there */
    /* Returns data block id */
    int blockNo = super.s_first_data_block; // 1?
    unsigned int size = 0;
    while (true) {
        if (!BM_ISSET(blockNo-1, blockBitmap)) {
            group.bg_free_blocks_count--;
            super.s_free_blocks_count--;
            BM_SET(blockNo-1, blockBitmap);
            lseek(image, BLOCK_OFFSET(super.s_first_data_block) + block_size * (blockNo - 1), SEEK_SET);
            write(image, block, block_size);
            std::cout << blockNo << " ";
            break;
        }
        if (size + block_size >= super.s_first_data_block + (super.s_blocks_count * block_size)) {
            // Block group border reached, Change group
            containingBgId = (super.s_first_data_block + blockNo - 1) / super.s_blocks_per_group;
            changeBlockGroup(containingBgId, false);
            size = 0;
        }
        else {
            size += block_size;
            blockNo++;
        }
    }
    return blockNo;
}

size_t actualDirEntrySize(unsigned int name_len) {
    return (8 + name_len + 3) & (~3);
}

size_t actualDirEntrySize(struct ext2_dir_entry* entry) {
    return (8 + entry->name_len + 3) & (~3);
}