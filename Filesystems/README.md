# Filecopy

The goal for this implementation is writing a program that can read a regular file (can be in any form) and copy its content as a new file into an ext2 image under a target folder, without mounting it.

Therefore an implementation that travels ext2 image byte-by-byte according to [ext2 structure](https://www.nongnu.org/ext2-doc/ext2.html) and copies the source file under the target directory as a new file 
provided with multiple block group and Triple Indirect block support (yeah you can actually copy a veeery large amount of stuff into an ext2 image with this implementation :smile:). 

#### Keywords
```
filesystem, fsck, ext2, inode, double-triple-indirect block
```

## Usage
```
git clone https://github.com/ysyesilyurt/OperatingSystems-2019.git
cd OperatingSystems-2019
make all
./filecopy <ext2_image_name> <source_file> <target_path/inode>
```
After the last line, given source file will be copied under given target path in the given ext2 filesystem. As a target, one should give an existing path in the given image of course and 
target can be specified as an absolute path in ext2 image or its inode number can be directly given (for example, instead of `/` one can use `2` which corresponds to root inode in ext2 filesystems). 

Given Makefile can provide you a sample `image.img` which is an ext2 image with a size of `1 MB`, 2 block groups, 1024 block size and 64 inodes. One can alter the parameters in `dd` and `mke2fs` to get ext2 images
with different features. Also please note that _`make all` creates this first image (`image.img`) for you._ 

One can mount and unmount the image file using `make mount`, `make umount` rules in Makefile respectively.

There are some source files for testing purposes under ```tests``` folder. Unfortunately I could not push the sample source file for testing triple indirect blocks since it requires a size greater than `65 MB`.

