#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// This file creates test files for xcheck.
// The base disk image is a filesystem containing the navy seal copypasta
// split into 16 short text files (one per sentence) and two empty directories.
// This program creates that base image and also other variations that are 
// subtly wrong in ways that xcheck will catch.


/*
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

// The following definitions are paraphrased from the xv6 project

#define BSIZE       (512)  // block size
#define INODE_START  (32)  // first inode block index
#define DATA_START   (59)  // first inode block index

typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned char  uchar;

typedef struct {
  u32 size;         // Size of file system image (blocks)
  u32 nblocks;      // Number of data blocks
  u32 ninodes;      // Number of inodes.
  u32 nlog;         // Number of log blocks
  u32 logstart;     // Block number of first log block
  u32 inodestart;   // Block number of first inode block
  u32 bmapstart;    // Block number of first free map block
} superblock;

#define NDIRECT 12
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
typedef struct {
  u16 type;           // File type
  u16 major;          // Major device number (T_DEV only)
  u16 minor;          // Minor device number (T_DEV only)
  u16 nlink;          // Number of links to inode in file system
  u32 size;            // Size of file (bytes)
  u32 addrs[NDIRECT+1];   // Data block addresses
} dinode; // 64 bytes, so there are 8 per block

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

typedef struct {
  u16  inum;
  char name[DIRSIZ];
} dirent; // 16 bytes, so there are 32 per block

u32 xint(u32 x);
u16 xshort(u16 x);

superblock sb;

// Our base file system has 20 inodes:
//      1 for the empty first inode
//      1 for the root directory
//     16 text files
//      2 for empty directories
dinode inodes[20];

dirent root_dir_data[20];

// These will be copied into dirent structs and put in the data block
// referenced by the root directory.
char *file_names[20] = {
       ".",       "..", "10th.txt", "11th.txt", 
"12th.txt", "13th.txt", "14th.txt", "15th.txt", 
"16th.txt",  "1st.txt",  "2nd.txt",  "3rd.txt",
 "4th.txt",  "5th.txt",  "6th.txt",  "7th.txt",
 "8th.txt",  "9th.txt", "empty_dir", "other_empty_di"
};

char *files_data[20] = {
  NULL, // first inode is empty - no data
  NULL, // see `root_dir_data`

  " You're fucking dead, kid.\n",

  " I can be anywhere, anytime, and I can kill you in over seven hundred ways, and " \
  "that's just with my bare hands.\n",

  "Not only am I extensively trained in unarmed combat, but I have access to the " \
  "entire arsenal of the United States Marine Corps and I will use it to its full " \
  "extent to wipe your miserable ass off the face of the continent, you little shit.\n",

  " If only you could have known what unholy retribution your little “clever” comment " \
  "was about to bring down upon you, maybe you would have held your fucking tongue.\n",

  "But you couldn't, you didn't, and now you're paying the price, you goddamn idiot.\n",

  " I will shit fury all over you and you will drown in it.\n",

  "You're fucking dead, kiddo.\n",

  "What the fuck did you just fucking say about me, you little bitch?\n",

  " I'll have you know I graduated top of my class in the Navy Seals, and I've been " \
  "involved in numerous secret raids on Al-Quaeda, and I have over 300 confirmed kills.\n",

  "I am trained in gorilla warfare and I'm the top sniper in the entire US armed forces.\n",

  " You are nothing to me but just another target.\n",

  " I will wipe you the fuck out with precision the likes of which has never been seen " \
  "before on this Earth, mark my fucking words.\n",

  "You think you can get away with saying that shit to me over the Internet?\n",

  " Think again, fucker.\n",

  " As we speak I am contacting my secret network of spies across the USA and your IP " \
  "is being traced right now so you better prepare for the storm, maggot.\n",

  "The storm that wipes out the pathetic little thing you call your life.\n",

  NULL, NULL // empty directories have no data
};

// global state
int block_index = 0;
int byte_index = 0;
int fsfd;

int write_block_or_die(char buf[BSIZE], char *message);
int write_bytes(void *bytes, size_t size);
int write_zero_block();
int write_string_block(char *str);
int copy_base_img(char *dest_file_name);

int main(int argc, char **argv) {
  // initialize the superblock
  sb = (superblock) {
    xint(1000),          // size
    xint(941),           // nblocks
    xint(200),           // ninodes
    xint(30),            // nlog
    xint(2),             // logstart
    xint(INODE_START),   // inodestart
    xint(58)             // bmapstart
  };

  // initialize the first inode (empty)
  inodes[0] = (dinode) { 0 };

  // initialize the root directory inode 
  inodes[1] = (dinode) {
    xshort(1),   // type: directory
    0,
    0,
    xshort(1),   // nlink 
    xint(512), // size. Directories are 1 block
    {xint(59), 0, 0, 0, 
            0, 0, 0, 0,
            0, 0, 0, 0,
            0} // addrs
  };

  // initialize the text file inodes.
  for (int i = 2; i < 18; i++) {
    inodes[i] = (dinode) {
      xshort(2),   // type: file
      0,
      0,
      xshort(1),   // nlink 
      xint(strlen(files_data[i])), // size 
      {xint(59 + i - 1), 0, 0, 0, 
                      0, 0, 0, 0,
                      0, 0, 0, 0,
                      0} // addrs
    };
  }

  // initialize the empty sub-directory inodes
  for (int i = 18; i < 20; i++) {
    inodes[i] = (dinode) {
      xshort(2),   // type: file. For some reason, mkfs doesn't understand directories.
      0,
      0,
      xshort(1),   // nlink 
      0, // size 
      {0, 0, 0, 0, 
       0, 0, 0, 0,
       0, 0, 0, 0,
       0} // addrs
    };
  }

  // intialize the data block for the root directory
  root_dir_data[0] = (dirent) { xshort(1), { '.' } };
  root_dir_data[1] = (dirent) { xshort(1), { '.', '.' } };
  for (int i = 2; i < 20; i++) {
    root_dir_data[i].inum = xshort(i);
    memcpy(root_dir_data[i].name, file_names[i], strlen(file_names[i]));
  }

  // Now that our data is intialized, it's time to start writing to the disk image file

  fsfd = open("./tests/3.img", O_RDWR|O_CREAT|O_TRUNC, 0666);
  if(fsfd < 0){
    perror(argv[1]);
    exit(1);
  }

  // write the first zero block
  assert(1 * BSIZE == write_zero_block());

  // write the superblock
  assert(2 * BSIZE == write_bytes(&sb, sizeof(superblock)));

  // log is empty for mkfs, so write zero blocks
  for (int i = 0; i < 30; i++) {
    write_zero_block();
  }
  assert(32 * BSIZE == byte_index);

  // write the inodes in batches of 8 (since there are 8 inodes per block)
  assert(33 * BSIZE == write_bytes(inodes, BSIZE));
  assert(34 * BSIZE == write_bytes(inodes + 8, BSIZE));
  assert(35 * BSIZE == write_bytes(inodes + 16, 4 * sizeof(dinode)));

  // fill the rest of the inode blocks with zeroes
  for (int i = 3; i < 26; i++) {
    write_zero_block();
  }
  assert(58 * BSIZE == byte_index);

  // write the block bitmap
  // I made it a char array just for convenience of visualization.                 
  char bitmap[10] = { 0xFF, 0xFF, 0xFF, 0xFF,
                      0xFF, 0xFF, 0xFF, 0xFF,
                      0xFF, 0x0F };
  assert(59 * BSIZE == write_bytes(bitmap, 10 * sizeof(char)));

  // write the data block for the root directory
  assert(60 * BSIZE == write_bytes(root_dir_data, 20 * sizeof(dirent)));

  // write the data blocks for the text files
  for (int i = 2; i < 18; i++) {
    write_string_block(files_data[i]);
  }
  assert(76 * BSIZE == byte_index);

  // and we're done. The empty dirs don't have data blocks

  // fill the rest of the file system with zeroes
  for (int i = 76; i < 1000; i++) {
    write_zero_block();
  }
  assert(1000 * BSIZE == byte_index);
  assert(1000 == block_index);

  // Now that we're done making the base case disk image (tests/3.img),
  // It's time to subtly break that disk image to make the other tests.
  


  // TEST 4: bad inode
  int test_fd = copy_base_img("./tests/4.img");
  // file byte offset for the 12th inode struct
  int inode_type_offset = INODE_START * BSIZE + 12 * sizeof(dinode);
  assert(inode_type_offset == lseek(test_fd, inode_type_offset, 0));
  u16 bad_inode_type = 0xAAAA;
  // write two bytes over the location of the u16 for the inode type
  assert(2 == write(test_fd, (void *) (&bad_inode_type), 2));
  assert(0 == close(test_fd));

  // TEST 5: bad direct address in inode (free in bitmap)
  test_fd = copy_base_img("./tests/5.img");
  // This is where the 2nd direct address of the root inode is stored
  int root_addr_offset = INODE_START * BSIZE + sizeof(dinode) + 16; // 4 u16s, 2 u32s
  assert(root_addr_offset == lseek(test_fd, root_addr_offset, 0));
  u32 bad_inode_addr = 500; // out of a possible 999
  assert(4 == write(test_fd, (void *) (&bad_inode_addr), 4));
  assert(0 == close(test_fd));

  // TEST 6: bad direct address in inode (in meta block)
  test_fd = copy_base_img("./tests/6.img");
  // This is where the 2nd direct address of the root inode is stored
  assert(root_addr_offset == lseek(test_fd, root_addr_offset, 0));
  bad_inode_addr = 2; // out of a possible 999
  assert(4 == write(test_fd, (void *) (&bad_inode_addr), 4));
  assert(0 == close(test_fd));

  // TEST 7: bad direct address in inode (> 1000)
  test_fd = copy_base_img("./tests/7.img");
  // This is where the 2nd direct address of the root inode is stored
  assert(root_addr_offset == lseek(test_fd, root_addr_offset, 0));
  bad_inode_addr = 1000; // out of a possible 999
  assert(4 == write(test_fd, (void *) (&bad_inode_addr), 4));
  assert(0 == close(test_fd));

  // TEST 8: bad indirect address in inode (free in bitmap)
  test_fd = copy_base_img("./tests/8.img");
  // This is where the 2nd direct address of the root inode is stored
  int root_indirect_addr_offset = INODE_START * BSIZE + sizeof(dinode) 
                                + 4 * sizeof(u16) 
                                + sizeof(u32)
                                + (NDIRECT * sizeof(u32)); 
  assert(root_indirect_addr_offset == lseek(test_fd, root_indirect_addr_offset, 0));
  bad_inode_addr = 500; // out of a possible 999
  assert(4 == write(test_fd, (void *) (&bad_inode_addr), 4));
  assert(0 == close(test_fd));

  // TEST 9: bad indirect address in inode (in meta block)
  test_fd = copy_base_img("./tests/9.img");
  // This is where the 2nd direct address of the root inode is stored
  root_indirect_addr_offset = INODE_START * BSIZE + sizeof(dinode) 
                                + 4 * sizeof(u16) 
                                + sizeof(u32)
                                + (NDIRECT * sizeof(u32)); 
  assert(root_indirect_addr_offset == lseek(test_fd, root_indirect_addr_offset, 0));
  bad_inode_addr = 2; // out of a possible 999
  assert(4 == write(test_fd, (void *) (&bad_inode_addr), 4));
  assert(0 == close(test_fd));

  // TEST 10: bad indirect address in inode (> 1000)
  test_fd = copy_base_img("./tests/10.img");
  // This is where the 2nd direct address of the root inode is stored
  root_indirect_addr_offset = INODE_START * BSIZE + sizeof(dinode) 
                                + 4 * sizeof(u16) 
                                + sizeof(u32)
                                + (NDIRECT * sizeof(u32)); 
  assert(root_indirect_addr_offset == lseek(test_fd, root_indirect_addr_offset, 0));
  bad_inode_addr = 1000; // out of a possible 999
  assert(4 == write(test_fd, (void *) (&bad_inode_addr), 4));
  assert(0 == close(test_fd));

  // TEST 11: root directory does not exist (parent links to wrong inode number)
  test_fd = copy_base_img("./tests/11.img");
  int root_parent_dirent_offset = DATA_START * BSIZE + sizeof(dirent);
  assert(root_parent_dirent_offset == lseek(test_fd, root_parent_dirent_offset, 0));
  u16 bad_inode_num = 0xAAAA; // should be 1 in proper filesystem
  assert(2 == write(test_fd, (void *) (&bad_inode_num), 2));
  assert(0 == close(test_fd));
  
  // TEST 8: directory not properly formatted
  // TEST 9: address used by inode but marked free in bitmap
  // TEST 10: bitmap marks block in use but it is not in use
  // TEST 11: direct address used more than once
  // TEST 12: indirect address used more than once
  // TEST 13: inode marked use but not found in a directory
  // TEST 14: inode referred to in directory but marked free
  // TEST 15: bad reference count for file
  // TEST 16: directory appears more than once in file system
  

  return 0;
}


// converts a u16 to little endian
u16 xshort(u16 x) {
  u16 y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

// converts a u32 to little endian
u32 xint(u32 x) {
  u32 y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

int write_block_or_die(char buf[BSIZE], char *message) {
  if (write(fsfd, buf, BSIZE) != BSIZE){
    perror(message);
    exit(1);
  }
  byte_index += BSIZE;
  block_index += 1;
  return byte_index;
}

int write_bytes(void *bytes, size_t size) {
  char buf[BSIZE] = { 0 };
  if (size > BSIZE) {
    perror("size too long to write in one block");
    exit(1);
  }
  memcpy(buf, bytes, size);
  return write_block_or_die(buf, "write bytes");
}

int write_zero_block() {
   char buf[BSIZE] = { 0 };
   return write_block_or_die(buf, "write zero block");
}

int write_string_block(char *str) {
  char buf[BSIZE] = { 0 };
  int len = strlen(str);
  if (len > BSIZE) {
    perror("string too long to write in one block");
    exit(1);
  }
  for (int i = 0; i < len; i++) {
    buf[i] = str[i];
  }
  return write_block_or_die(buf, "write string block");
}

// copies the base img (tests/1.img) into a new file at dest_file_name.
// returns the file descriptor of the destination file.
int copy_base_img(char *dest_file_name) {
  char buf[4096];
  ssize_t nread;

  int dest_fd = open(dest_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (dest_fd < 0) {
    perror("could not open destination file for copying");
    exit(1);
  }

  assert(0 == lseek(fsfd, 0, 0));
  while (nread = read(fsfd, buf, sizeof buf), nread > 0) {
    char *out_ptr = buf;
    ssize_t nwritten;

    do {
      nwritten = write(dest_fd, out_ptr, nread);

      if (nwritten >= 0) {
        nread -= nwritten;
        out_ptr += nwritten;
      }
    } while (nread > 0);
  }
  return dest_fd;
}
