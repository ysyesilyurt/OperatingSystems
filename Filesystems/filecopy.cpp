#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <iostream>

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

int resolveTargetInode(const char *);

struct ext2_super_block super;
struct ext2_group_desc group;
bmap* block_bitmap;
bmap* inode_bitmap;
int image;

int main(int argc, const char* argv[]) {
    struct ext2_super_block super;
    struct ext2_group_desc group;

    /* Open ext2 image file */

    /* Read parameters */
    const char * image = argv[1];
    const char * sourcefile = argv[2];
    const char * target = argv[3];
    int targetInode = resolveTargetInode(target);

    /* allocate a new inode and fill its meta as sourcefile's (use mmap) */
        // checking inode bitmap blocks
    /* allocate a new data blocks(required amount of them) for that new inode */
        // checking data bitmap blocks
    /* dont forget to set these new data blocks as the new inode's (directs)*/
    /* Copy the data from sourcefile to new inode's data blocks */

    /* go to targetInode and get its directory data blocks */
    /* insert at dir entry in these data blocks for mapping of new inode to this dir */


    /* Close ext2 image file*/

    return 0;
}

int resolveTargetInode(const char * target) {
    int num = atoi(target);
    if (num)
        return num;
    else if (strncmp("/", target, 1))
        return 2;
    else {
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


        return 0;
    }
}
