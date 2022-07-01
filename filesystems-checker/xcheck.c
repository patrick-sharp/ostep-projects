#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>

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

#define BSIZE (512)
#define T_DIR   (1)
#define T_FILE  (2)
#define T_DEV   (3)

#define SB_START      (1)
#define LOG_START     (2)
#define INODE_START  (32)
#define BMAP_START   (58)
#define DATA_START   (59)
#define NUM_BLOCKS (1000)

#define INODE_SIZE  (64) // in bytes
#define DIRENT_SIZE (16) // in bytes
#define NDIRECT     (12) // number of direct addresses per inode
#define NINODES    (200) // number of inodes

typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned char  u8;

u32 xint(u32 x);
u16 xshort(u16 x);

bool isNthBit1 (void *bitmap, int n);

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: xcheck <file_system_image>\n");
    exit(1);
  }
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    if (errno == ENOENT) {
      fprintf(stderr, "image not found.\n");
    } else {
      perror("could not open image");
    }
    exit(1);
  }
  struct stat statbuf;
  assert(0 == fstat(fd, &statbuf));
  char *file_bytes = mmap(NULL,statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
  assert(file_bytes != MAP_FAILED);
  close(fd);
  
  // ERROR: bad inode
  for (int i = BSIZE * INODE_START; i < BMAP_START * BSIZE; i += INODE_SIZE) {
    u16 *inode_type_p = (u16 *) (file_bytes + i);
    u16 inode_type = xshort(*inode_type_p);
    if (   inode_type != 0 // unallocated
        && inode_type != T_DIR
        && inode_type != T_FILE
        && inode_type != T_DEV ) {
      fprintf(stderr, "ERROR: bad inode.\n");
      exit(1);
    }
  }


  char *bitmap = file_bytes + BSIZE * BMAP_START;
  for (int i = 0; i < NINODES; i++) {
    // ERROR: bad direct address in inode.
    for (int j = 0; j < NDIRECT; j++) {
      u32 direct_addr = xint(((u32 *) (file_bytes + INODE_START * BSIZE + i * INODE_SIZE + 12))[j]);
      // since unallocated blocks are 0 and the bitmap is all 1s for the meta blocks, this works
      if (   (direct_addr != 0 && direct_addr < DATA_START) 
          || direct_addr > NUM_BLOCKS 
          || !isNthBit1(bitmap, direct_addr)) {
        fprintf(stderr, "ERROR: bad direct address in inode.\n");
        exit(1);
      }
    }

    // ERROR: bad indirect address in inode.
    u32 indirect_addr = ((u32 *) (  file_bytes 
                                  + INODE_START * BSIZE 
                                  + i * INODE_SIZE 
                                  + 4 * sizeof(u16)
                                  + sizeof(u32)
          ))[NDIRECT];
    if (   (indirect_addr != 0 && indirect_addr < DATA_START) 
        || indirect_addr > NUM_BLOCKS 
        || !isNthBit1(bitmap, indirect_addr)) {
      fprintf(stderr, "ERROR: bad indirect address in inode.\n");
      exit(1);
    }
  }

  // ERROR: root directory does not exist.
  int root_self_dirent_offset = DATA_START * BSIZE;
  u16 root_self_inum = xshort(*((u16 *) (file_bytes + root_self_dirent_offset)));
  int root_parent_dirent_offset = DATA_START * BSIZE + DIRENT_SIZE;
  u16 root_parent_inum = xshort(*((u16 *) (file_bytes + root_parent_dirent_offset)));
  if (root_self_inum != 1 || root_parent_inum != 1) {
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }

  // ERROR: directory not properly formatted.
  for (int i = 0; i < NINODES; i++) {
    void *ptr = file_bytes + INODE_START * BSIZE + i * INODE_SIZE;
    u16 type = xshort(*((u16 *) ptr));
    if (type == T_DIR) {
      ptr += 4 * sizeof(u16) + sizeof(u32);
      u32 addr = xint(*((u32 *) ptr));
      ptr = file_bytes + addr * BSIZE;
      u16 inum = xshort(*((u16 *) ptr));
      ptr += sizeof(u16);
      char *self_name = (char *) (ptr);
      ptr += DIRENT_SIZE;
      char *parent_name = (char *) (ptr);
      if (   inum != i 
          || strcmp(self_name, ".") != 0 
          || strcmp(parent_name, "..") != 0) {
        fprintf(stderr, "ERROR: directory not properly formatted.\n");
        exit(1);
      }
    }
  }


  assert(0 == munmap(file_bytes, statbuf.st_size));
  return 0;
}

// swaps endian-ness of x
u16 xshort(u16 x) {
  u16 y;
  u8 *a = (u8*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

// swaps endian-ness of x
u32 xint(u32 x) {
  u32 y;
  u8 *a = (u8*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

bool isNthBit1(void *bitmap, int n) {
  u8 byte = ((u8 *) bitmap)[n/8];
  //printf("%i %i ", byte, 0x1 << (n%8));
  return ((byte & (0x1 << (n%8))) > 0);
}
