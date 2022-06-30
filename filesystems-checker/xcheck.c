#include <stdio.h>
#include <stdint.h>

/*

FSSIZE:  1000 blocks in whole file system
BSIZE:    512 bytes per block
NINODES:  200 inodes
NDIRECT:   12 maximum direct links per file
IPB:        8 inodes per block
LOGSIZE:   30 blocks (not used for this project)

sizeof(struct dinode): 64 bytes
inode types: 1 (Directory), 2 (File), 3 (Device)

DIRSIZ: 14 files max per directory
sizeof(struct dirent): 16 bytes


nbitmap = 1       // Number of bitmap blocks
ninodeblocks = 25 // Number of inode blocks
nlog = 30;        // Number of log blocks
nmeta = 59        // Number of meta blocks (boot, sb, nlog, inode, bitmap)
nblocks = 941     // Number of data blocks

IBLOCK(i) gets the index of the file system block that contains the i'th inode.


wsect(sec, buf) writes from the block buffer to the sec'th block of the file system
winode(inum, ip) writes the inode at ip into the inum'th inode entry of the file system
rinode(inum, ip) reads the inode at the inum'th inode entry of the file system into inode struct at ip
rsect(sec, buf) reads data from the sec'th block of the file system to the block buffer
ialloc(type) make space in the inode block for an inode of the given type, returns allocated inode number
balloc(used) marks every block in the block bitmap as allocated, up to the used'th bit of the bitmap
iappend(inum, xp, n) appends the directory entry at xp to the inum'th inode. n is the number of bytes to write


Layout of the disk image:
The disk image is little-endian
Block 0 (bytes 0-512)
  First block is empty
Block 1 (bytes 513-1024)
  2nd block contains the superblock struct
    size: 1000 (4 bytes)
    nblocks: 941 (4 bytes)
    ninodes: 200 (4 bytes)
    nlog: 30 (4 bytes)
    logstart: 2 (4 bytes)
    inodestart: 32 (4 bytes)
    bmapstart: 58 (4 bytes)
Blocks 2-31 (bytes 1025-16384)
  These blocks are for the log
Blocks 32-57 (bytes 16385-29696)
  These blocks are where the inodes are stored.
  Since the root inode number is 1, the first slot for an inode is empty.
  The first inode will be at byte 16384 + 64 = 16448.
Block 58 (bytes 29697-30208)
  This block is for the block bitmap. Xv6 does not have an inode bitmap.
Blocks 59-999 (bytes 30209-512000)
  This is where the actual data blocks are stored.
*/

int main(int argc, char **argv) {
  return 0;
}
