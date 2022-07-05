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
void set_nth_bit_0(void *bitmap, int n);
void set_nth_bit_1(void *bitmap, int n);
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

  int num_direct_addrs = 0;
  u32 direct_addrs[FSSIZE];
  int num_indirect_addrs = 0;
  u32 indirect_addrs[FSSIZE];
  u8 used_inodes_bitmap[NINODES / 8] = { 0 };
  u8 inode_references[NINODES] = { 0 };

  // list out all the direct and indirect addresses
  // list out all the inode numbers in directories
  // list out all the inode numbers whose type != 0
  inode_references[1]++; // make sure the root directory counts as referenced
  for (int i = 0; i < NINODES; i++) {
    dinode *ip = get_nth_inode(i);
    u16 type = xshort(ip->type);
    if (type == 0) {
      continue;
    } else if (type == T_DIR) {
      for (int j = 2; j < BSIZE / sizeof(dirent); j++) {
        // we start at 2 because the first two entries of a dirent
        // are "." and ".."
        dirent *de = get_nth_dirent(ip, j);
        u16 inum = xshort(de->inum);
        if (inum != 0) {
          inode_references[inum]++;
        }
      }
    }

    set_nth_bit_1(used_inodes_bitmap, i);

    for (int j = 0; j < NDIRECT; j++) {
      u32 direct_addr = xint(ip->addrs[j]);
      if (direct_addr != 0) {
        direct_addrs[num_direct_addrs++] = direct_addr;
      }
    }

    u32 indirect_block_addr = xint(ip->addrs[NDIRECT]);
    indirect_addrs[num_indirect_addrs++] = indirect_block_addr;
    if (is_addr_in_bounds(indirect_block_addr)) {
      u32 *indirect_block = file_bytes + indirect_block_addr * BSIZE;
      for (int j = 0; j < BSIZE / sizeof(u32); j++) {
        u32 indirect_addr = xint(indirect_block[j]);
        if (indirect_addr != 0) {
          indirect_addrs[num_indirect_addrs++] = indirect_addr;
        }
      }
    }
  }

  // ERROR: bad direct address in inode.
  for (int i = 0; i < num_direct_addrs; i++) {
    if (!is_addr_in_bounds(direct_addrs[i])) {
      fprintf(stderr, "ERROR: bad direct address in inode.\n");
      exit(1);
    }
  }

  // ERROR: bad indirect address in inode.
  for (int i = 0; i < num_indirect_addrs; i++) {
    if (!is_addr_in_bounds(indirect_addrs[i])) {
      fprintf(stderr, "ERROR: bad indirect address in inode.\n");
      exit(1);
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

  // ERROR: address used by inode but marked free in bitmap.
  char *bitmap = file_bytes + BMAPSTART * BSIZE;
  for (int i = 0; i < num_direct_addrs; i++) {
    if (!is_nth_bit_1(bitmap, direct_addrs[i])) {
      fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
      exit(1);
    }
  }
  for (int i = 0; i < num_indirect_addrs; i++) {
    if (!is_nth_bit_1(bitmap, indirect_addrs[i])) {
      fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
      exit(1);
    }
  }

  // ERROR: bitmap marks block in use but it is not in use.
  u8 used_bitmap [BSIZE / 8];
  memcpy(used_bitmap, bitmap, BSIZE / 8);
  for (int i = 0; i < num_direct_addrs; i++) {
    set_nth_bit_0(used_bitmap, direct_addrs[i]);
  }
  for (int i = 0; i < num_indirect_addrs; i++) {
    set_nth_bit_0(used_bitmap, indirect_addrs[i]);
  }
  if (used_bitmap[7] != 0x07) {
     fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
     exit(1);
  }
  for (int i = 8; i < BSIZE / 8; i++) {
    if (used_bitmap[i] != 0) {
       fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
       exit(1);
    }
  }

  // ERROR: direct address used more than once.
  for (int i = 0; i < num_direct_addrs - 1; i++) {
    for (int j = i + 1; j < num_direct_addrs; j++) {
      if (direct_addrs[i] == direct_addrs[j]) {
        fprintf(stderr, "ERROR: direct address used more than once.\n");
        exit(1);
      }
    }
  }
  for (int i = 0; i < num_direct_addrs; i++) {
    for (int j = 0; j < num_indirect_addrs; j++) {
      if (direct_addrs[i] == indirect_addrs[j]) {
        fprintf(stderr, "ERROR: direct address used more than once.\n");
        exit(1);
      }
    }
  }

  // ERROR: indirect address used more than once.
  for (int i = 0; i < num_indirect_addrs - 1; i++) {
    for (int j = i + 1; j < num_indirect_addrs; j++) {
      if (indirect_addrs[i] == indirect_addrs[j]) {
        fprintf(stderr, "ERROR: indirect address used more than once.\n");
        exit(1);
      }
    }
  }

  // ERROR: inode marked use but not found in a directory.
  // ERROR: inode referred to in directory but marked free.
  for (int i = 0; i < NINODES; i++) {
    bool nth_inode_used = is_nth_bit_1(used_inodes_bitmap, i);
    bool nth_inode_referenced = inode_references[i] > 0;
    if (nth_inode_used && !nth_inode_referenced) {
      fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
      exit(1);
    } else if (!nth_inode_used && nth_inode_referenced) {
      fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
      exit(1);
    }
  }

  // ERROR: bad reference count for file.
  for (int i = 0; i < NINODES; i++) {
    dinode *ip = get_nth_inode(i);
    if (xshort(ip->type) == T_FILE && xshort(ip->nlink) != inode_references[i]) {
      fprintf(stderr, "ERROR: bad reference count for file.\n");
      exit(1);
    }
  }

  // ERROR: directory appears more than once in file system.
  for (int i = 0; i < NINODES; i++) {
    dinode *ip = get_nth_inode(i);
    if (xshort(ip->type) == T_DIR && 
        (xshort(ip->nlink) > 1 || inode_references[i] > 1)) { 
      fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
      exit(1);
    }
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

void set_nth_bit_0(void *bitmap, int n) {
  ((u8 *) bitmap)[n/8] &= ~(0x1 << (n%8)); 
}

void set_nth_bit_1(void *bitmap, int n) {
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
