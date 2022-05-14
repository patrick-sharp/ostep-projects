#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

bool isVerbose = true;

void printVerbose(char *fmt, ...) {
  if (isVerbose) {
    va_list args;
    va_start(args,fmt);
    vprintf(fmt, args);
    va_end(args);
  }
}

void die_if(bool condition, char *fmt, ...) {
  if (condition) {
    va_list args;
    va_start(args,fmt);
    printf("Error: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    exit(1);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("%s: file1 [file2 ...]", argv[0]);
  }
  
  // iterate over each filepath in argv
  for (int i = 1; i < argc; i++) {
    char *filepath = argv[i];

    int fd = open(filepath, O_RDONLY);
    die_if(fd < 0, "Could not open %s", filepath);

    struct stat statbuf;
    int err = fstat(fd, &statbuf);
    die_if(err < 0, "Could not get stats on %s", filepath);

    char *ptr = mmap(NULL,statbuf.st_size, PROT_READ|PROT_WRITE,MAP_SHARED, fd, 0);
    die_if(ptr == MAP_FAILED, "mmap failed on %s", filepath);

    err = munmap(ptr, statbuf.st_size);
    die_if(err != 0, "munmap failed on %s", filepath);

    close(fd);
  }

  return 0;
}
