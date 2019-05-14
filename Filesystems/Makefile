CC = g++

all: clean create compile run

compile: filecopy.cpp ext2.h
		$(CC) filecopy.cpp -o filecopy

create:
		dd if=/dev/zero of=image.img bs=1024 count=128
		mke2fs -N 32 image.img

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