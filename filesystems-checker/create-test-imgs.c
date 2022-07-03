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
void make_test_file(bool should_succeed, char *error, int offset, void *new_bytes, size_t new_bytes_len);

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

  /*// write the inodes in batches of 8 (since there are 8 inodes per block)
  assert(33 * BSIZE == write_bytes(inodes, BSIZE));
  assert(34 * BSIZE == write_bytes(inodes + 8, BSIZE));
  assert(35 * BSIZE == write_bytes(inodes + 16, 4 * sizeof(dinode)));*/

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
  int offset;

  error = "ERROR: bad inode.\n";
  // file byte offset for the 3rd inode struct (for hex.txt)
  offset = INODESTART * BSIZE + 3 * sizeof(dinode);
  u16 bad_inode_type = 0xAAAA;
  make_test_file(false, error, offset, &bad_inode_type, sizeof(u16));

  error = "ERROR: bad direct address in inode.\n";
  u32 bad_inode_addr;
  // Direct address points to bitmap block
  // first direct address of root
  offset = INODESTART * BSIZE + sizeof(dinode) + offsetof(dinode, addrs); 
  bad_inode_addr = BMAPSTART; 
  make_test_file(false, error, offset, &bad_inode_addr, sizeof(u16));
  // Direct address points to block >= 1000 (out of 999)
  // 4th direct address of letters.txt
  offset = INODESTART * BSIZE + 3 * sizeof(dinode) + offsetof(dinode, addrs) + 3 * sizeof(u32);
  bad_inode_addr = 1000; 
  make_test_file(false, error, offset, &bad_inode_addr, sizeof(u16));
  // Direct address of unused inode points to block >= 1000 (out of 999)
  // 4th direct address of random inode 
  offset = INODESTART * BSIZE + 20 * sizeof(dinode) + offsetof(dinode, addrs) + 3 * sizeof(u32);
  bad_inode_addr = 0x00ABCDEF; 
  make_test_file(true, "", offset, &bad_inode_addr, sizeof(u16));

  /*error = "ERROR: bad indirect address in inode.\n";
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

  // TEST 8: address used by inode but marked free in bitmap.
  // This one is out of order because I accidentally completed it as part of "bad direct address in inode"
  test_fd = copy_base_img("./tests/8.img");
  // This is where the 2nd direct address of the root inode is stored
  int root_indirect_addr_offset = INODESTART * BSIZE + sizeof(dinode) 
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
  root_indirect_addr_offset = INODESTART * BSIZE + sizeof(dinode) 
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
  root_indirect_addr_offset = INODESTART * BSIZE + sizeof(dinode) 
                                + 4 * sizeof(u16) 
                                + sizeof(u32)
                                + (NDIRECT * sizeof(u32)); 
  assert(root_indirect_addr_offset == lseek(test_fd, root_indirect_addr_offset, 0));
  bad_inode_addr = 1000; // out of a possible 999
  assert(4 == write(test_fd, (void *) (&bad_inode_addr), 4));
  assert(0 == close(test_fd));

  // TEST 11: root directory does not exist (parent links to wrong inode number)
  test_fd = copy_base_img("./tests/11.img");
  int root_parent_dirent_offset = DATASTART * BSIZE + sizeof(dirent);
  assert(root_parent_dirent_offset == lseek(test_fd, root_parent_dirent_offset, 0));
  u16 bad_inode_num = 0xAAAA; // should be 1 in proper filesystem
  assert(2 == write(test_fd, (void *) (&bad_inode_num), 2));
  assert(0 == close(test_fd));
  
  // TEST 12: directory not properly formatted
  test_fd = copy_base_img("./tests/12.img");
  int root_parent_name_offset = DATASTART * BSIZE + sizeof(dirent) + sizeof(u16);
  assert(root_parent_name_offset == lseek(test_fd, root_parent_name_offset, 0));
  char *bad_inode_name = "...";
  assert(3 == write(test_fd, (void *) (bad_inode_name), 3));
  assert(0 == close(test_fd));*/

  // TEST 13: bitmap marks block in use but it is not in use
  /*test_fd = copy_base_img("./tests/13.img");
  int root_parent_name_offset = DATASTART * BSIZE + sizeof(dirent) + sizeof(u16);
  assert(root_parent_name_offset == lseek(test_fd, root_parent_name_offset, 0));
  char *bad_inode_name = "...";
  assert(3 == write(test_fd, (void *) (bad_inode_name), 3));
  assert(0 == close(test_fd));*/
  // TEST 11: direct address used more than once
  // TEST 12: indirect address used more than once
  // TEST 13: inode marked use but not found in a directory
  // TEST 14: inode referred to in directory but marked free
  // TEST 15: bad reference count for file
  // TEST 16: directory appears more than once in file system
  

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
  //printf("%d %s %s\n", test_counter, filename, filedata);
  int fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);
  assert(-1 < fd);
  int len = strlen(filedata);

  assert(len == write(fd, filedata, len));
  assert(0 == close(fd));
}

void make_test_file(bool should_succeed, char *error, int offset, void *new_bytes, size_t new_bytes_len) {
  char filename[16];
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
