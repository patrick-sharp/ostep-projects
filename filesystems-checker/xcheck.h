/*
Layout of the disk image:
The disk image is little-endian
Block 0 (bytes 0-511)
  First block is empty
Block 1 (bytes 512-1023)
  2nd block contains the superblock struct
    size: 1000 (4 bytes)
    nblocks: 941 (4 bytes)
    ninodes: 200 (4 bytes)
    nlog: 30 (4 bytes)
    logstart: 2 (4 bytes)
    inodestart: 32 (4 bytes)
    bmapstart: 58 (4 bytes)
Blocks 2-31 (bytes 1024-16383)
  These blocks are for the log
Blocks 32-57 (bytes 16384-29695)
  These blocks are where the inodes are stored.
  Since the root inode number is 1, the first slot for an inode is empty.
  The first inode will be at byte 16384 + 64 = 16448.
Block 58 (bytes 29696-30207)
  This block is for the block bitmap. Xv6 does not have an inode bitmap.
Blocks 59-999 (bytes 30208-511999)
  This is where the actual data blocks are stored.
*/

// int types
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

// superblock data
#define FSSIZE   (1000)
#define NBLOCKS   (941) // number of data blocks
#define NINODES   (200)
#define NLOG       (30)
#define LOGSTART    (2)
#define INODESTART (32)
#define BMAPSTART  (58)

// implied superblock data
#define SBSTART     (1)
#define DATASTART  (59)
#define NMETA      (59)

#define BSIZE (512)

#define NDIRECT (12)
#define NINDIRECT (BSIZE / sizeof(u32))
#define MAXFILE (NDIRECT + NINDIRECT)

// inode types
#define T_DIR   (1)
#define T_FILE  (2)
#define T_DEV   (3)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ (14) // max chars per filename

typedef struct {
  u32 size;         // Size of file system image (blocks)
  u32 nblocks;      // Number of data blocks
  u32 ninodes;      // Number of inodes.
  u32 nlog;         // Number of log blocks
  u32 logstart;     // Block number of first log block
  u32 inodestart;   // Block number of first inode block
  u32 bmapstart;    // Block number of first free map block
} superblock;

// On-disk inode structure
typedef struct {
  u16 type;               // File type
  u16 major;              // Major device number (for T_DEV inodes only)
  u16 minor;              // Minor device number (for T_DEV inodes only)
  u16 nlink;              // Number of links to inode in file system
  u32 size;               // Size of file (bytes)
  u32 addrs[NDIRECT+1];   // Data block addresses
} dinode; // 64 bytes, so there are 8 per block

typedef struct {
  u16  inum;
  char name[DIRSIZ];
} dirent; // 16 bytes, so there are 32 per block


// Functions for swapping endian-ness of different size ints
u16 xshort(u16 x) {
  u16 y;
  u8 *a = (u8*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

u32 xint(u32 x) {
  u32 y;
  u8 *a = (u8*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}
