#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
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
unsigned int writeToBlock(unsigned char *);

struct ext2_super_block super;
struct ext2_group_desc group;
bmap* blockBitmap;
bmap* inodeBitmap;
int image, sFile;

int main(int argc, char* argv[]) {

    /* Open ext2 image file */
    if ((image = open(argv[1], O_RDWR)) < 0) {
        fprintf(stderr, "Could not open img file RDWR\n");
        return 1;
    }

    /* Read superblock into super */
    lseek(image, BASE_OFFSET, SEEK_SET); // SEEK_SET => start seeking from the beginning of file
    read(image, &super, sizeof(super));
    if (super.s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Img file could not be classified as a ext2 fs\n");
        return 1;
    }
    block_size = 1 << (10 + super.s_log_block_size); // blksz => 2^{10 + super.logblocksize}

    blockBitmap = new bmap[block_size];
    inodeBitmap = new bmap[block_size];

    /* Read Zeroth Group descriptor into group and read Bitmaps accordingly with BG */
    changeBlockGroup(0, true);

    /* Read target */
    int targetInodeNo = resolveTargetInode(argv[3]);
    // std::cout << targetInodeNo << "\n";

    /* Open sourcefile */
    if ((sFile = open(argv[2], O_RDONLY)) < 0) {
        fprintf(stderr, "Could not open source file RDONLY\n");
        return 2;
    }

    /* You are searching for an empty place for your inode */
    // get Starting block address of inode table from group ofc you need to get inode's group first etc. then get the address from that group
        // group desc table -> there is an entry for each group, which contains the absolute block addresses of data blocks bitmap, inodes bitmap, and inodes table.
    // then start searching from 12 (first 11 are reserved and indexing starts from 1) BE CARE INODESZ
        //  If valid, the value is non-zero. Each pointer is the block address of a block containing data for this inode.
        // make your search with BM_ISSET(index, inodeBitmap) etc.
        // if all reserved, change to a new group
        // If you find get its block id (__le32)?, use BM_SET
        // btw Maybe update some entries in other places like free inodes count in gdt etc.
    // lseek(image, group.bg_inode_table*EXT2_BLOCK_SIZE+(11*sizeof(tempInode)), SEEK_SET); // go to the start of inode table + 11 (i.e. 12th inode)
    // read(image, &tempInode, sizeof(tempInode)); // read 12th inode

    /* allocate a new inode and fill its meta as sourcefile's */
    struct ext2_inode newInode;

    /* Place new inode into next empty place for inodes */
    // go to first non-reserved inode
    const unsigned int inodeNo = super.s_first_ino; // super.s_first_ino + 1 ??
    unsigned int containingBgId = (inodeNo - 1) / super.s_inodes_per_group;
    unsigned int newInodeOffset = BLOCK_OFFSET(group.bg_inode_table) + (inodeNo - 1) * sizeof(struct ext2_inode);
    // change group desc if needed
    changeBlockGroup(containingBgId, false);
    // increment next available inodeNo
    super.s_first_ino++;
    BM_SET(inodeNo, inodeBitmap);
    group.bg_free_inodes_count--;
    super.s_free_inodes_count--;
    lseek(image, newInodeOffset, SEEK_SET);
    read(image, &newInode, sizeof(struct ext2_inode));
    fillMeta(&newInode, sFile);

    /* allocate new data blocks(required amount of them) for that new inode */
    /* and copy the data from sourcefile to new inode's data blocks */
    /* You are searching for an empty place for your data blocks */
    // get s_first_data_block, i.e. 32bit value identifying the first data block, in other word the id of the block containing the superblock structure.
    // then start searching BE CARE BLKSZ
        // make your search with BM_ISSET(index, blockBitmap) etc.
        // if all reserved, change to a new group
        // If you find empty place for data block i get its block id (__le32) and put it to newInode's i_block and use BM_SET =>  Data block addresses in inode structure are absolute block addresses of the filesystem image.
        // do this till you copy all the data blocks

    /* Copy sourcefile contents to new data blocks for new inode */
    unsigned char block[block_size];
    unsigned int size = 0, directCounter = 0;
    unsigned int sFilesize = lseek(sFile, (off_t) 0, SEEK_END);
    while (size <= sFilesize && directCounter < 12) {
        /* Read block_size many bytes from sFile to block and write it to First non-reserved data block */
        lseek(sFile, block_size, SEEK_SET);
        read(sFile, block, block_size);
        // set new data block as inode's
        newInode.i_block[directCounter] = writeToBlock(block);
        size += block_size;
        directCounter++;
    }

    /* insert a dir entry in these data blocks for mapping of new inode to this dir */
        // if no place for new dir entry at the last data block, alloc a new one with searching for a free block using SETs etc

    /* go to targetInode and get its directory data blocks */
    containingBgId = (targetInodeNo - 1) / super.s_inodes_per_group;
    // change group desc if needed
    changeBlockGroup(containingBgId, false);
    // go to targetInode
    struct ext2_inode targetInode;
    lseek(image, BLOCK_OFFSET(group.bg_inode_table) + (targetInodeNo - 1) * sizeof(struct ext2_inode), SEEK_SET);
    read(image, &targetInode, sizeof(struct ext2_inode));
    // Traverse targetInode's data blocks
    // innerFlag = false;
    bool found = false;
    for (int i = 0; i < 12 ; ++i) {
        lseek(image, BLOCK_OFFSET(targetInode.i_block[i]), SEEK_SET);
        read(image, block, block_size);
        // convert data block to entry
        struct ext2_dir_entry* entry = (struct ext2_dir_entry*)block;

        /* allocate a new dirEntry struct ext2_dir_entry* dirEntry = (struct ext2_dir_entry*) malloc(sizeof(ext2_dir_entry)) */

        // Traverse dir entries of data block i
        size = 0;
        while (size <= block_size) { // if entry->inode => switch to a new data block
            if (!entry->inode) {
                /* Found an empty entry */
                /* Fill its data */
                entry->inode = inodeNo;
                entry->name_len = EXT2_NAME_LEN;
                entry->file_type = EXT2_FT_REG_FILE;
                strncpy(entry->name, argv[2], entry->name_len);
                entry->rec_len = sizeof(ext2_dir_entry); // 16bit unsigned displacement to the next directory entry from the start of the current directory entry ????????*/
                found = true;
                break;
            }
            /* move to the next entry */
            size += entry->rec_len;
            entry = (ext2_dir_entry*)((char*)entry + entry->rec_len);
        }
        if (found) {
            /* Write changes to in dir entry to image */
            BM_SET(targetInode.i_block[i], blockBitmap); // ?
            lseek(image, BLOCK_OFFSET(targetInode.i_block[i]), SEEK_SET);
            write(image, block, block_size);
            break;
        }
    }

    /* Write inode to img */
    lseek(image, newInodeOffset, SEEK_SET);
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

    /* writing to file */
        // checking inode bitmap blocks
        // /seek()-write() !!!!!
        // checking data bitmap blocks

        /*    lseek(fd,
          BLOCK_OFFSET(group.bg_inode_table) +
          (inode_no - 1) * sizeof(struct ext2_inode),
          SEEK_SET);
    write(fd, &inode, sizeof(struct ext2_inode));*/


    // FIX THE STRUCTURE LATER!!

    // DELETE NEWs ?

    /* Close ext2 image file*/
    close(image);

    return 0;
}

int resolveTargetInode(char * target) {
    int num = atoi(target);
    if (num)
        return num;
    else if (*target == '/')
        return 2;
    else {
        printf("%s\n", target);
        /* Construct a vector of paths */
        std::vector<char*> path;
        char temp[255];
        strncpy(temp, target, 255);
        char *delim = "/";
        char *ptr = strtok(temp, delim);
        while(ptr) {
            path.push_back(ptr);
            ptr = strtok(NULL, delim);
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
                    if (entry->name == path[pathIndex]) {
                        /* Found the entry in current dir hierarchy */
                        if (pathIndex == path.size()) {
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
        return 0;
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
    inode->i_blocks = fileStat.st_blocks / 2; // st_blocks gives number of 512B blocks allocated, we have a block size 1K
    inode->i_atime = fileStat.st_atime;
    inode->i_mtime = fileStat.st_mtime;
    inode->i_ctime = fileStat.st_ctime;
}

void changeBlockGroup(int bgID, bool first) {

    if (!first) {
        /* First Overwrite changes in bitmaps into image */
        lseek(image, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
        write(image, inodeBitmap, block_size);

        lseek(image, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
        write(image, blockBitmap, block_size);
    }

    /* Then change BG */
    lseek(image, BASE_OFFSET+sizeof(super)+sizeof(group)*bgID, SEEK_SET);
    read(image, &group, sizeof(group));

    /* and change bitmaps */
    lseek(image, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET);
    read(image, blockBitmap, block_size);

    lseek(image, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET);
    read(image, inodeBitmap, block_size);
}

unsigned int writeToBlock(unsigned char * block) {
    /* Finds new available data block and writes data in "block" to there */
    /* Returns data block id */
    int blockNo = 1;
    unsigned int size = 0;
    while (true) {
        if (!BM_ISSET(blockNo - 1, blockBitmap)) {
            group.bg_free_blocks_count--;
            super.s_free_blocks_count--;
            BM_SET(blockNo - 1, blockBitmap);
            lseek(image, BLOCK_OFFSET(super.s_first_data_block) + block_size * (blockNo - 1), SEEK_SET);
            write(image, block, block_size);
            break;
        }
        if (size + block_size > super.s_first_data_block + (super.s_blocks_count * block_size)) {
            // Block group border reached, Change group
            unsigned int containingBgId = (super.s_first_data_block + blockNo - 1) / super.s_blocks_per_group; // (blockNo - 1) ?
            changeBlockGroup(containingBgId, false);
            size = 0;
        }
        else {
            size += block_size;
            blockNo++;
        }
    }
    return BLOCK_OFFSET(super.s_first_data_block) + block_size * (blockNo - 1);
}
