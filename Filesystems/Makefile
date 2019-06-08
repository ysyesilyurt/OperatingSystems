CC = g++

all: clean create compile

compile: filecopy.cpp ext2.h
		$(CC) filecopy.cpp -o filecopy

create:
		dd if=/dev/zero of=image.img bs=1024 count=1024
		mke2fs -N 64 -g 512 image.img

mount:
		mkdir mnt
		sudo mount -o loop image.img mnt/

umount:
		sudo umount mnt
		rm -rf mnt

clean:
		sudo rm -rf image.img filecopy

run:
		./filecopy image.img file.txt /