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

#include "xcheck.h"

dinode *get_nth_inode(int n);
dirent *get_nth_dirent(dinode *dinode_p, int n);
char *get_bitmap();

bool is_nth_bit_1(void *bitmap, int n);
bool is_addr_in_bounds(u32 addr);

void *file_bytes;

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
  file_bytes = mmap(NULL,statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
  assert(file_bytes != MAP_FAILED);
  close(fd);
  
  // ERROR: bad inode
  for (int i = 0; i < NINODES; i++) {
    dinode *ip = get_nth_inode(i);
    u16 inode_type = xshort(ip->type);
    if (   inode_type != 0 // unallocated
        && inode_type != T_DIR
        && inode_type != T_FILE
        && inode_type != T_DEV ) {
      fprintf(stderr, "ERROR: bad inode.\n");
      exit(1);
    }
  }


  for (int i = 0; i < NINODES; i++) {
    dinode *ip = get_nth_inode(i);
    if (xshort(ip->type) == 0) {
      continue;
    }
     // ERROR: bad direct address in inode.
    for (int j = 0; j < NDIRECT; j++) {
      u32 direct_addr = xint(ip->addrs[j]);
      // since unallocated blocks are 0 and the bitmap is all 1s for the meta blocks, this works
      if (!is_addr_in_bounds(direct_addr)) {
        fprintf(stderr, "ERROR: bad direct address in inode.\n");
        exit(1);
      }
    }

    // ERROR: bad indirect address in inode.
    u32 indirect_addr = xint(ip->addrs[NDIRECT]);
    if (indirect_addr != 0) {
      if (!is_addr_in_bounds(indirect_addr)) {
        fprintf(stderr, "ERROR: bad indirect address in inode.\n");
        exit(1);
      }
      u32 *indirect_block = file_bytes + indirect_addr * BSIZE;
      for (int j = 0; j < BSIZE / sizeof(u32); j++) {
        if (!is_addr_in_bounds(indirect_block[j])) {
          fprintf(stderr, "ERROR: bad indirect address in inode.\n");
          exit(1);
        }
      }
    }
  }

  // ERROR: root directory does not exist.
  dinode *root_ip = get_nth_inode(1);
  if (xshort(root_ip->type) != T_DIR) {
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }
  dirent *root_self_dirent = get_nth_dirent(root_ip, 0);
  dirent *root_parent_dirent = root_self_dirent + 1;
  u16 root_self_inum = xshort(root_self_dirent->inum);
  u16 root_parent_inum = xshort(root_parent_dirent->inum);
  if (root_self_inum != 1 || root_parent_inum != 1) {
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }

  // ERROR: directory not properly formatted.
  for (int i = 0; i < NINODES; i++) {
    dinode *ip = get_nth_inode(i);
    u16 type = xshort(ip->type);
    if (type == T_DIR) {
      dirent *dp0 = get_nth_dirent(ip, 0);
      u16 inum = xshort(dp0->inum);
      char *self_name = dp0->name;
      dirent *dp1 = get_nth_dirent(ip, 1);
      char *parent_name = dp1->name;

      if (inum != i || 
          strcmp(self_name, ".") != 0 || 
          strcmp(parent_name, "..") != 0) {
        fprintf(stderr, "ERROR: directory not properly formatted.\n");
        exit(1);
      }
    }
  }

  // ERROR: bitmap marks block in use but it is not in use.
  // NMETA = 59, so bitmap is 0xFFFFFFFF FFFFFF07
  // 59 meta blocks + 1 root block + 16 file blocks = 76
  u8 used_bitmap [BSIZE / 8];
  for (int i = 0; i < NMETA / 8; i++) {
    used_bitmap[i] = 0xFF;
  }
  used_bitmap[NMETA/8] = 0x07;
  for (int i = 0; i < NINODES; i++) {
    //inode *ip = get_nth_inode(i);
    for (int j = 0; j < NDIRECT + 1; j++) {
      //u32 addr = xint(ip->addrs[j]);
    }
    //u32 ind_addr = xint(ip->addrs[NDIRECT]);
  }
  

  assert(0 == munmap(file_bytes, statbuf.st_size));
  return 0;
}

char *get_bitmap() {
  return file_bytes + BSIZE * BMAPSTART;
}

bool is_nth_bit_1(void *bitmap, int n) {
  u8 byte = ((u8 *) bitmap)[n/8];
  return ((byte & (0x1 << (n%8))) > 0);
}

bool is_addr_in_bounds(u32 addr) {
  return addr == 0 || (addr >= DATASTART && addr < FSSIZE);
}

void set_nth_bit_1(void *bitmap, int n) {
  //u8 byte = ((u8 *) bitmap)[n/8];
  ((u8 *) bitmap)[n/8] |= (0x1 << (n%8)); 
}

dinode *get_nth_inode(int n) {
  assert(n >= 0 && n < 200);
  return (dinode *) (file_bytes + INODESTART * BSIZE + n * sizeof(dinode));
}

dirent *get_nth_dirent(dinode *inode_p, int n) {
  assert(n >= 0); // I'm not sure what the upper bound on directories is.
  assert(xshort(inode_p->type) == T_DIR);
  return (dirent *) (file_bytes + xshort(inode_p->addrs[0]) * BSIZE + n * sizeof(dirent));
}
