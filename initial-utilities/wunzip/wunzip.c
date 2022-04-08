#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256

// returns 0 on success, 1 on failure
int readEntry(FILE *fp) {
  int numChars;
  char currChar;
  if (fread(&numChars, sizeof(int), 1, fp) != 1
      || fread(&currChar, sizeof(char), 1, fp) != 1) {
    return 1;
  }
  for (int i = 0; i < numChars; i++) {
    printf("%c", currChar);
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 1) {
    printf("wunzip: file1 [file2 ...]\n");
    return 1;
  } else {

    for (int i = 1; i < argc; i++) {
      char *filename = argv[i];
      FILE* fp = fopen(filename, "r");
      if (fp == NULL) {
        printf("wunzip: cannot open file\n");
        return 1;
      }

      while (readEntry(fp) != 1) {}

      int close = fclose(fp);
      if (close != 0) {
        printf("wunzip: cannot close file\n");
        return 1;
      }
    }
  }
}

