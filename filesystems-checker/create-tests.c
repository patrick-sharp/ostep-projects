#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xcheck.h"

// This file creates test files for xcheck.
// The base disk image is a filesystem containing a root directory 
// with two files. The first file is called "hex.txt", and it contains 
// 16 blocks. Each block is 512 copies of one hexadecimal digit. I.E. 
// this file is "00000...", "11111...", ... "FFFFF...".
// The second file is called "letters.txt". It contains 26 blocks. 
// Each block is 512 copies of one lowercase letter of the alphabet.
// I chose these files because they are easy to see when you open the
// disk image in a hex editor, and because they require the filesystem
// to use indirect addresses. Without indirect addresses, we can't test
// some of the functionality of xcheck.
//
// This program creates that base image and also other variations that are 
// subtly wrong in ways that xcheck will catch.

int open_test_file();
void copy_base_img(int dest_fd);
void make_test_file(bool should_succeed, char *desc, char *error, int offset, void *new_bytes, size_t new_bytes_len);

int write_block_or_die(char buf[BSIZE], char *message);
int write_bytes(void *bytes, size_t size);
int write_zero_block();
int write_string_block(char *str);
int write_char_block(char c);
int write_char_block_with_newline(char c);

// global state
int block_index = 0;
int byte_index = 0;
int fsfd;
int test_counter = 3; // the first two are about calling xcheck correctly,
                      // not about whether it properly catches errors.

int main(int argc, char **argv) {
  // initialize the superblock
  superblock sb = (superblock) {
    xint(FSSIZE),  
    xint(NBLOCKS), 
    xint(NINODES),
    xint(NLOG),
    xint(LOGSTART),
    xint(INODESTART),
    xint(BMAPSTART)
  };

  // Now that our data is intialized, it's time to start writing to the disk image file
  fsfd = open_test_file();
  if(fsfd < 0){
    perror(argv[1]);
    exit(1);
  }

  // write the first zero block
  assert(SBSTART * BSIZE == write_zero_block());

  // write the superblock
  assert(LOGSTART * BSIZE == write_bytes(&sb, sizeof(superblock)));

  // log is empty for mkfs, so write 30 zero blocks
  for (int i = 0; i < NLOG; i++) {
    write_zero_block();
  }
  assert(INODESTART * BSIZE == byte_index);

  // write in an empty inode and our three inodes
  dinode inodes[4] = { 
    // empty inode
    (dinode) {},

    // root inode
    (dinode) {
      xshort(T_DIR),      // type
      0, 0,               // major and minor (not relevant)
      xshort(1),          // nlink
      xint(BSIZE),        // size (directories are one block)
      { xint(DATASTART) } // addrs (root dir gets first block)
    },

    // hex.txt
    (dinode) {
      xshort(T_FILE), // type
      0, 0,           // major and minor (not relevant)
      xshort(1),      // nlink
      xint(8192),     // size (512 * 16 = 8192)
      { 0 }           // addrs 
    },

    // letters.txt
    (dinode) {
      xshort(T_FILE), // type
      0, 0,           // major and minor (not relevant)
      xshort(1),      // nlink
      xint(13312),    // size (512 * 26 = 13312)
      { 0 }           // addrs 
    }
  };

  // 12 direct blocks
  //  1 indirect block
  //  4 children of the indirect block
  for (int i = 0; i < NDIRECT + 1; i++) {
    inodes[2].addrs[i] = xint(DATASTART + i + 1);
  }

  // 12 direct blocks
  //  1 indirect block
  // 14 children of the indirect block
  // The `+ 19` comes from
  // 1 block of root dir
  // 17 blocks of hex.txt
  for (int i = 0; i < NDIRECT + 1; i++) {
    inodes[3].addrs[i] = xint(DATASTART + i + 18);
  }

  assert((INODESTART + 1) * BSIZE == write_bytes(inodes, 4 * sizeof(dinode)));

  // fill the rest of the inode blocks with zeroes
  for (int i = block_index; i < BMAPSTART; i++) {
    write_zero_block();
  }
  assert(BMAPSTART * BSIZE == byte_index);

  // write the block bitmap
  // I made it a char array just for convenience of visualization.                 
  u8 bitmap[13] = { 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF };
  assert(DATASTART * BSIZE == write_bytes(bitmap, 13 * sizeof(u8)));

  // write the root directory's dirents
  dirent dirents[4] = {
    (dirent) {
      xshort(1),
      { '.' }
    },
    (dirent) {
      xshort(1),
      { '.', '.' }
    },
    (dirent) {
      xshort(2),
      { 'h', 'e', 'x', '.', 't', 'x', 't' }
    },
    (dirent) {
      xshort(3),
      { 'l', 'e', 't', 't', 'e', 'r', 's', '.', 't', 'x', 't' }
    }
  };

  assert((DATASTART + 1) * BSIZE == write_bytes(dirents, sizeof(dirents)));

  // hex.txt
  // write the direct blocks for hex.txt
  for (char c = '0'; c <= '9'; c++) {
    write_char_block(c);
  }
  for (char c = 'A'; c <= 'B'; c++) {
    write_char_block(c);
  }
  // write the indirect block for hex.txt
  u32 hex_addrs[4];
  for (int i = 0; i < 4; i++) {
    hex_addrs[i] = block_index + 1 + i;
  }
  write_bytes(hex_addrs, sizeof(hex_addrs));
  // write the children of the indirect block for hex.txt
  for (char c = 'C'; c < 'F'; c++) {
    write_char_block(c);
  }
  write_char_block_with_newline('F');

  assert (DATASTART + 18 == block_index);
  
  // letters.txt
  // write the direct blocks for letters.txt
  for (char c = 'a'; c <= 'l'; c++) {
    write_char_block(c);
  }
  // write the indirect block for letters.txt
  u32 letters_addrs[14];
  for (int i = 0; i < 14; i++) {
    letters_addrs[i] = block_index + 1 + i;
  }
  // write the children of the indirect block for letters.txt
  write_bytes(letters_addrs, sizeof(letters_addrs));
  for (char c = 'm'; c < 'z'; c++) {
    write_char_block(c);
  }
  write_char_block_with_newline('z');
  assert(DATASTART + 45 == block_index);

  // fill the rest of the file system with zeroes
  for (int i = block_index; i < FSSIZE; i++) {
    write_zero_block();
  }
  assert(FSSIZE * BSIZE == byte_index);
  assert(FSSIZE == block_index);

  // Now that we're done making the base case disk image (tests/3.img),
  // It's time to make some copies and edit them to make the other tests.
  char *error;
  char *desc;
  int offset;
  u16 bad_inode_type;
  u32 bad_inode_addr;
  u16 bad_inode_num;
  char *bad_inode_name;
  u8 bad_bitmap_byte;
  dinode bad_inode;
  dirent bad_dirent;


  error = "ERROR: bad inode.\n";
  desc = "Root inode has type = 0xAAAA\n";
  // file byte offset for the 3rd inode struct (for hex.txt)
  offset = INODESTART * BSIZE + 3 * sizeof(dinode);
  bad_inode_type = 0xAAAA;
  make_test_file(false, desc, error, offset, &bad_inode_type, sizeof(u16));


  error = "ERROR: bad direct address in inode.\n";
  desc = "First direct address of root is BMAPSTART\n";
  // first direct address of root
  offset = INODESTART * BSIZE + sizeof(dinode) + offsetof(dinode, addrs); 
  bad_inode_addr = xint(BMAPSTART); 
  make_test_file(false, desc, error, offset, &bad_inode_addr, sizeof(u32));
  desc = "4th direct address of letters.txt inode is 1000 (out of 999)\n";
  // 4th direct address of letters.txt
  offset = INODESTART * BSIZE + 3 * sizeof(dinode) + offsetof(dinode, addrs) + 3 * sizeof(u32);
  bad_inode_addr = xint(FSSIZE); 
  make_test_file(false, desc, error, offset, &bad_inode_addr, sizeof(u32));
  //
  desc = "4th direct address of unused inode is 0xABCDEF\n";
  // 4th direct address of random inode 
  offset = INODESTART * BSIZE + 20 * sizeof(dinode) + offsetof(dinode, addrs) + 3 * sizeof(u32);
  bad_inode_addr = xint(0x00ABCDEF); 
  make_test_file(true, desc, "", offset, &bad_inode_addr, sizeof(u32));


  error = "ERROR: bad indirect address in inode.\n";
  desc = "Address to indirect block of hex.txt is BMAPSTART\n";
  // address to indirect block of hex.txt
  offset = INODESTART * BSIZE + 2 * sizeof(dinode) + offsetof(dinode, addrs) + NDIRECT * sizeof(u32); 
  bad_inode_addr = xint(BMAPSTART); 
  make_test_file(false, desc, error, offset, &bad_inode_addr, sizeof(u32));
  desc = "Address to indirect block of hext.txt is 1000\n";
  // address to indirect block of hex.txt
  offset = INODESTART * BSIZE + 2 * sizeof(dinode) + offsetof(dinode, addrs) + NDIRECT * sizeof(u32); 
  bad_inode_addr = xint(FSSIZE); 
  make_test_file(false, desc, error, offset, &bad_inode_addr, sizeof(u32));
  desc = "Indirect address of unused inode is 0xABCDEF\n";
  // address to indirect block of random inode 
  offset = INODESTART * BSIZE + 20 * sizeof(dinode) + offsetof(dinode, addrs) + NDIRECT * sizeof(u32);
  bad_inode_addr = xint(0x00ABCDEF); 
  make_test_file(true, desc, "", offset, &bad_inode_addr, sizeof(u32));
  desc = "Address in indirect block is BMAPSTART\n";
  // 4th address in indirect block of hex.txt
  offset = (DATASTART + 30) * BSIZE + 2 * sizeof(u32); 
  bad_inode_addr = xint(BMAPSTART); 
  make_test_file(false, desc, error, offset, &bad_inode_addr, sizeof(u32));
  desc = "4th address in indirect block of hex.txt is 1000\n";
  // 4th address in indirect block of hex.txt
  offset = (DATASTART + 30) * BSIZE + 2 * sizeof(u32); 
  bad_inode_addr = xint(FSSIZE); 
  make_test_file(false, desc, error, offset, &bad_inode_addr, sizeof(u32));


  error = "ERROR: root directory does not exist.\n";
  desc = "Root inode has type 0 (unallocated)\n";
  // set root inode's type to 0
  offset = INODESTART * BSIZE + sizeof(dinode) + offsetof(dinode, type); 
  bad_inode_type = 0; 
  make_test_file(false, desc, error, offset, &bad_inode_type, sizeof(u16));
  desc = "Root inode has parent inode number set to 2\n";
  // set root inode's parent to something other than itself.
  offset = DATASTART * BSIZE + sizeof(dirent) + offsetof(dirent, inum);
  bad_inode_num = xshort(2); 
  make_test_file(false, desc, error, offset, &bad_inode_num, sizeof(u16));


  error = "ERROR: directory not properly formatted.\n";
  desc = "Root directory contains no \".\" entry";
  offset = DATASTART * BSIZE + offsetof(dirent, name);
  bad_inode_name = "bad inode name";
  make_test_file(false, desc, error, offset, &bad_inode_name, strlen(bad_inode_name));
  desc = "Root directory's \".\" entry has inode number set to 2";
  offset = DATASTART * BSIZE + offsetof(dirent, name);
  bad_inode_num = 2;
  make_test_file(false, desc, error, offset, &bad_inode_num, sizeof(u16));


  error = "ERROR: address used by inode but marked free in bitmap.\n";
  desc = "4th indirect address of letters.txt is marked free\n";
  // 4th indirect address of letters.txt is 0x5D, which is bit 5 of byte 11 in the bitmap
  offset = BMAPSTART * BSIZE + 11;
  bad_bitmap_byte = 0b11111011;
  make_test_file(false, desc, error, offset, &bad_bitmap_byte, sizeof(u8));
  desc = "Address to indirect block of hex.txt is marked free\n";
  // Address to indirect block of hex.txt is 0x48, which is bit 0 of byte 9 in the bitmap
  offset = BMAPSTART * BSIZE + 9;
  bad_bitmap_byte = 0b01111111;
  make_test_file(false, desc, error, offset, &bad_bitmap_byte, sizeof(u8));
  desc = "4th indirect address of hex.txt is marked free\n";
  // 4th indirect address of hex.txt is 0x4C, which is bit 4 of byte 9 in the bitmap
  offset = BMAPSTART * BSIZE + 9;
  bad_bitmap_byte = 0b11110111;
  make_test_file(false, desc, error, offset, &bad_bitmap_byte, sizeof(u8));


  error = "ERROR: bitmap marks block in use but it is not in use.\n";
  desc = "Byte 14 of bitmap is 0b10000000, marking free address 104 (0x68) as in use \n";
  offset = BMAPSTART * BSIZE + 14;
  bad_bitmap_byte = 0b10000000;
  make_test_file(false, desc, error, offset, &bad_bitmap_byte, sizeof(u8));

  
  error = "ERROR: direct address used more than once.\n";
  desc = "Address 59 is used more than once (direct addr of new inode)\n";
  offset = INODESTART * BSIZE + 4 * sizeof(dinode); 
  bad_inode = (dinode) {
      xshort(T_FILE),     // type
      0, 0,               // major and minor (not relevant)
      xshort(1),          // nlink
      0,                  // size (not relevant)
      { xint(DATASTART) } // addrs (same as root dir, but a file)
  };


  error = "ERROR: indirect address used more than once.\n";
  desc = "Address 89 (0x59) is used more than once\n";
  offset = INODESTART * BSIZE + 4 * sizeof(dinode); 
  bad_inode = (dinode) {
      xshort(T_FILE), // type
      0, 0,           // major and minor (not relevant)
      xshort(1),      // nlink
      0,              // size (not relevant)
      { 0 }           // addrs 
  };
  bad_inode.addrs[NDIRECT] = xint(0x59);
  make_test_file(false, desc, error, offset, &bad_inode, sizeof(dinode));
  

  error = "ERROR: inode marked use but not found in a directory.\n";
  desc = "hex.txt is deleted from the root directory, but still exists as a used inode\n";
  offset = DATASTART * BSIZE + 2 * sizeof(dirent);
  bad_dirent = (dirent) {};
  make_test_file(false, desc, error, offset, &bad_dirent, sizeof(dirent));


  error = "ERROR: inode referred to in directory but marked free.\n";
  desc = "letters.txt is moved from inode 3 to inode 4 free but still referenced as inode 3\n";
  offset = INODESTART * BSIZE + 3 * sizeof(dinode);
  dinode bad_inode_pair[2] = {
    {},
    inodes[3]
  };
  make_test_file(false, desc, error, offset, &bad_inode_pair, sizeof(bad_inode_pair));


  error = "ERROR: bad reference count for file.\n";
  desc = "letters.txt has nlink set to 2\n";
  offset = INODESTART * BSIZE + 3 * sizeof(dinode);
  bad_inode = inodes[3];
  bad_inode.nlink = xshort(2);
  make_test_file(false, desc, error, offset, &bad_inode, sizeof(dinode));

  
  error = "ERROR: directory appears more than once in file system.\n";
  desc = "root directory has nlink set to 2\n";
  offset = INODESTART * BSIZE + sizeof(dinode);
  bad_inode = inodes[1];
  bad_inode.nlink = xshort(2);
  make_test_file(false, desc, error, offset, &bad_inode, sizeof(dinode));

  assert(0 == close(fsfd));
  return 0;
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

int write_char_block(char c) {
  char buf[BSIZE];
  memset(buf, c, BSIZE);
  return write_block_or_die(buf, "write char block");
}

int write_char_block_with_newline(char c) {
  char buf[BSIZE];
  memset(buf, c, BSIZE);
  buf[BSIZE - 1] = 0x0A; // newline ascii code
  return write_block_or_die(buf, "write char block with newline");
}

// returns fd of opened file
int open_test_file() {
  char filename[16];
  assert(0 < snprintf(filename, sizeof(filename), "./tests/%d.img", test_counter));
  test_counter++;
  return open(filename, O_RDWR|O_CREAT|O_TRUNC, 0666);
}

// copies the base img (tests/3.img) into a new file with 
// descriptor dest_fd
void copy_base_img(int dest_fd) {
  char buf[4096];
  ssize_t nread;

  assert(0 == lseek(fsfd, 0, 0));
  while (nread = read(fsfd, buf, sizeof buf), nread > 0) {
    char *out_ptr = buf;
    ssize_t nwritten;

    do {
      nwritten = write(dest_fd, out_ptr, nread);
      assert(-1 < nwritten);

      if (nwritten >= 0) {
        nread -= nwritten;
        out_ptr += nwritten;
      }
    } while (nread > 0);
  }
}

void make_file_with(char *filename, char *filedata) {
  int fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);
  assert(-1 < fd);
  int len = strlen(filedata);

  assert(len == write(fd, filedata, len));
  assert(0 == close(fd));
}

void make_test_file(bool should_succeed, char *desc, char *error, 
    int offset, void *new_bytes, size_t new_bytes_len) {
  char filename[16];
  // make the test #.desc file
  assert(0 < snprintf(filename, sizeof(filename), "./tests/%d.desc", test_counter));
  make_file_with(filename, desc);

  // make the test #.run file
  assert(0 < snprintf(filename, sizeof(filename), "./tests/%d.run", test_counter));
  char run_data[32];
  assert(0 < snprintf(run_data, sizeof(run_data), "./xcheck ./tests/%d.img\n", test_counter));
  make_file_with(filename, run_data);

  // make the test #.rc file
  assert(0 < snprintf(filename, sizeof(filename), "./tests/%d.rc", test_counter));
  if (should_succeed) {
    make_file_with(filename, "0\n");
  } else {
    make_file_with(filename, "1\n");
  }

  // make the test #.err file
  assert(0 < snprintf(filename, sizeof(filename), "./tests/%d.err", test_counter));
  make_file_with(filename, error);

  // make the test #.out file
  assert(0 < snprintf(filename, sizeof(filename), "./tests/%d.out", test_counter));
  make_file_with(filename, "");

  // make the test image. Do this last since it updates the test counter
  int img_fd = open_test_file();
  if (img_fd < 0) {
    perror("could not open destination file for copying");
    exit(1);
  }
  copy_base_img(img_fd);
  assert(offset == lseek(img_fd, offset, 0));
  assert(new_bytes_len == write(img_fd, new_bytes, new_bytes_len));
}
