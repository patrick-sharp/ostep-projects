#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *openAndCheck(char* filename, char* mode) {
  FILE *fp = fopen(filename, mode);
  if (fp == NULL) {
    fprintf(stderr, "reverse: cannot open file '%s'\n", filename);
    exit(1);
  }
  return fp;
}

void closeAndCheck(FILE *fp) {
    int close = fclose(fp);
    if (close != 0) {
      printf("reverse: cannot close file\n");
      exit(1);
    }
}

typedef struct Node {
  char *line;
  size_t lineLength;
  struct Node *next;
} Node;

void reverse(FILE *inputFile, FILE *outputFile) {
  Node* curr = NULL;
  while (1) {
    char *buffer = NULL;
    size_t capacity = 0;
    if (getline(&buffer, &capacity, inputFile) == -1) {
      break;
    }
    Node *newCurr = (Node *) malloc(sizeof(Node));
    if (newCurr == NULL) {
      fprintf(stderr, "reverse: malloc failed\n");
      exit(1);
    }
    newCurr->line = buffer;
    newCurr->lineLength = capacity;
    newCurr->next = curr;
    curr = newCurr;
  }

  while (curr != NULL) {
    fwrite(curr->line, sizeof(char), strlen(curr->line), outputFile);
    Node *temp = curr->next;
    free(curr);
    curr = temp;
  }
}

int main(int argc, char **argv) {
  FILE *inputFile;
  FILE *outputFile;

  if (argc == 1) {
    inputFile = stdin;
    outputFile = stdout;
  } else if (argc == 2) {
    inputFile = openAndCheck(argv[1], "r");
    outputFile = stdout;
  } else if (argc == 3) {
    char *inputFilename = argv[1];
    char *outputFilename = argv[2];
    char *p_basename = basename(inputFilename);
    char *heapInputBasename = (char *) malloc(strlen(p_basename));
    strcpy(heapInputBasename, p_basename);
    p_basename = basename(outputFilename);
    if (strcmp(heapInputBasename, p_basename) == 0) {
      fprintf(stderr, "reverse: input and output file must differ\n");
      free(heapInputBasename);
      return 1;
    }
    free(heapInputBasename);
    inputFile = openAndCheck(inputFilename, "r");
    outputFile = openAndCheck(outputFilename, "w");  
  } else {
    fprintf(stderr, "usage: reverse <input> <output>\n");
    return 1;
  }

  reverse(inputFile, outputFile);

  if (inputFile != stdin) {
    closeAndCheck(inputFile);
  }
  if (outputFile != stdout) {
    closeAndCheck(outputFile);
  }
  return 0;
}
