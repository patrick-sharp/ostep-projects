#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WHITESPACE " \t\n"

char error_message[30] = "An error has occurred\n";
char *initialPath = "/bin";
bool isVerbose = false;

void printVerbose(char *fmt, ...) {
  if (isVerbose) {
    va_list args;
    va_start(args,fmt);
    vprintf(fmt, args);
    va_end(args);
  }
}

void error() {
  fwrite(error_message, sizeof(char), strlen(error_message), stderr);
}

void exitError() {
  fwrite(error_message, sizeof(char), strlen(error_message), stderr);
  exit(1);
}

char *sepall(char **strp, char *sep) {
  char *token = *strp;
  do {
    token = strsep(strp, sep);
  } while (*strp != NULL && strlen(token) == 0);
  return token;
}

bool isCharInStr(char c, char *sep) {
  for (int i = 0; i < strlen(sep); i++) {
    if (c == sep[i]) {
      return true;
    }
  }
  return false;
}

int getNumArgs(char *str, char **redirectFileNamep) {
  int numTokens = 0;
  char prevWasSep = true;
  char hasRedirect = false;
  int i;
  for (i = 0; i < strlen(str); i++) {
    if (str[i] == '>') {
      hasRedirect = true;
      str[i] = ' '; // so that when we parse the args later, we only parse up to where the '>' was
      i += 1;
      break;
    }

    bool currIsSep = isCharInStr(str[i], WHITESPACE);
    // procs when we are at the start of a token
    if (prevWasSep && !currIsSep) {
      numTokens += 1;
    } 
    prevWasSep = currIsSep;
  }
  if (hasRedirect) {
    int numRedirectArgs = 0;
    int redirectFileNameStart = -1;
    int redirectFileNameEnd = -1;
    prevWasSep = true; // this is so that redirection doesn't require whitespace
    for (; i < strlen(str); i++) {
      // set currIsSep
      bool currIsSep = isCharInStr(str[i], WHITESPACE);
      // procs when we are at the start of a token
      if (prevWasSep && !currIsSep) {
        numRedirectArgs += 1;
        if (numRedirectArgs == 1) {
          redirectFileNameStart = i;
        } else if (numRedirectArgs > 1) {
          printVerbose("Too many args to redirect\n");
          error();
          return -1;
        }
      } else if (!prevWasSep && currIsSep) {
        redirectFileNameEnd = i;
      }
      prevWasSep = currIsSep;
    }
    if (numRedirectArgs == 0) {
      printVerbose("Not enough args to redirect\n");
      error();
      return -1;
    }

    if (redirectFileNameEnd != -1) {
      str[redirectFileNameEnd] = '\0';
    }

    *redirectFileNamep = strdup(str + redirectFileNameStart);
    return numTokens;
  } else {
    *redirectFileNamep = NULL;
    return numTokens;
  }
}

// returns pointer to array of numArgs + 1 elements.
// first numArgs elements are arguments.
// last element of array will always be NULL.
// assumes there are actually numArgs args to parse.
char **parseArgs(char **parallelCommandp, int numArgs) {
  char **args = (char **) malloc((numArgs + 1) * sizeof(char*));
  for (int i = 0; i < numArgs; i++) {
    char *arg = sepall(parallelCommandp, WHITESPACE);
    args[i] = arg;
  }
  args[numArgs] = NULL;
  return args;
}

bool assertArgs(char *progname, int expected, int actual) {
  if (expected != actual) {
    printVerbose("Wrong number of arguments to %s: %i (should be %i)\n", progname, actual - 1, expected - 1);
    error();
    return false;
  }
  return true;
};

void freePathArr(char ***pathsp, int numPathsp) {
  for (int i = 0; i < numPathsp; i++) {
    free((*pathsp)[i]);
  }
  free(*pathsp);
}

void reallocPathArr(char ***pathsp, int *numPathsp, int newNumPaths) {
  if (*pathsp != NULL) {
    freePathArr(pathsp, *numPathsp);
  }
  *pathsp = (char **) malloc(sizeof(char **) * newNumPaths);
  if (*pathsp == NULL) {
    printVerbose("malloc failed in reallocPathArr\n");
    error();
  }
  *numPathsp = newNumPaths;
}

void parseParallelCommand(char *parallelCommand, char ***pathsp, int *numPathsp) {
  char *redirectFileName = NULL;
  int numArgs = getNumArgs(parallelCommand, &redirectFileName);
  printVerbose("redirect file name: %s\n", redirectFileName);
  if (numArgs == -1) {
    return;
  } else if (numArgs == 0) {
    if (redirectFileName != NULL) {
      printVerbose("must supply a command to redirect\n");
      error();
    }
    return;
  }
  char **args = parseArgs(&parallelCommand, numArgs);
  if (strcmp(args[0], "cd") == 0) {
    if (!assertArgs("cd", 2, numArgs)) {
      return;
    }
    if (chdir(args[1]) != 0) {
      printVerbose("directory change error\n");
      error();
    }
    char *cwd = NULL;
    cwd = getcwd(cwd, 0);
    printVerbose("%s\n", cwd);
    free(cwd);
  } else if (strcmp(args[0], "exit") == 0) {
    if (!assertArgs("exit", 1, numArgs)) {
      return;
    }
    printVerbose("exit\n");
    exit(0);
  } else if (strcmp(args[0], "path") == 0) {
    reallocPathArr(pathsp, numPathsp, numArgs - 1);
    //printVerbose("# paths: %i, paths: ", *numPathsp);
    for (int i = 1; i < numArgs; i++) {
      (*pathsp)[i-1] = strdup(args[i]);
    }
    printVerbose("\n");
  } else {
    if (*numPathsp == 0) {
      printVerbose("No path - cannot find non-builtin command\n");
      error();
      return;
    }
    bool wasFileError = false;
    for (int i = 0; i < *numPathsp; i++) {
      char *fname = (char *) malloc(strlen((*pathsp)[i]) + 1 + strlen(args[0]) + 1);
      if (fname == NULL) {
        printVerbose("malloc failed for fname\n");
        error();
      }
      strcpy(fname, (*pathsp)[i]);
      strcat(fname, "/");
      strcat(fname, args[0]);
      printVerbose("binary file name: %s\n", fname);
      if (access(fname, F_OK) == 0) {
        // file exists
        int forkResult = fork();
        if (forkResult < 0) {
          printVerbose("fork failed\n");
          error();
        } else if (forkResult == 0) {
          if (redirectFileName != NULL) {
            int outFd = open(redirectFileName, O_WRONLY|O_APPEND|O_CREAT, 0644);
            if (outFd == -1) {
              printVerbose("Couldn't open/create output file %s\n", redirectFileName);
              printVerbose("%s\n", strerror(errno));
              wasFileError = true;
              error();
              break;
            }
            dup2(outFd, 1);
            dup2(outFd, 2);
            close(outFd);
          }
          if (execv(fname, args) == -1) {
            printVerbose("execv failed\n");
            error();
            exit(1);
          }
        } else {
          int waitResult = wait(NULL);
          if (waitResult == -1) {
            printVerbose("wait failed\n");
            error();
          }
        }
        break; // found the command, so we can stop looking
      } else if (i == *numPathsp - 1) {
        printVerbose("Binary not found\n");
        error();
      }
      free(fname);
    }
  }
  free(args);
}

void parseLine(char *lineBuffer, char ***pathsp, int *numPathsp) {
  do {
    char *parallelCommand = strsep(&lineBuffer, "&");
    parseParallelCommand(parallelCommand, pathsp, numPathsp);
    /*char *redirectFileName = NULL;
    int numArgs = getNumArgs(parallelCommand, &redirectFileName);
    printVerbose("redirect file name: %s\n", redirectFileName);
    if (numArgs == -1) {
      continue;
    } else if (numArgs == 0) {
      if (redirectFileName != NULL) {
        error();
      }
      continue;
    }
    char **args = parseArgs(&parallelCommand, numArgs);
    if (strcmp(args[0], "cd") == 0) {
      if (!assertArgs("cd", 2, numArgs)) {
        continue;
      }
      if (chdir(args[1]) != 0) {
        printVerbose("directory change error\n");
        error();
      }
      char *cwd = NULL;
      cwd = getcwd(cwd, 0);
      printVerbose("%s\n", cwd);
      free(cwd);
    } else if (strcmp(args[0], "exit") == 0) {
      if (!assertArgs("exit", 1, numArgs)) {
        continue;
      }
      printVerbose("exit\n");
      exit(0);
    } else if (strcmp(args[0], "path") == 0) {
      reallocPathArr(pathsp, numPathsp, numArgs - 1);
      //printVerbose("# paths: %i, paths: ", *numPathsp);
      for (int i = 1; i < numArgs; i++) {
        (*pathsp)[i-1] = strdup(args[i]);
      }
      printVerbose("\n");
    } else {
      if (*numPathsp == 0) {
        printVerbose("No path - cannot find non-builtin command\n");
        error();
        continue;
      }
      bool wasFileError = false;
      for (int i = 0; i < *numPathsp; i++) {
        char *fname = (char *) malloc(strlen((*pathsp)[i]) + 1 + strlen(args[0]) + 1);
        if (fname == NULL) {
          printVerbose("malloc failed for fname\n");
          error();
        }
        strcpy(fname, (*pathsp)[i]);
        strcat(fname, "/");
        strcat(fname, args[0]);
        printVerbose("binary file name: %s\n", fname);
        if (access(fname, F_OK) == 0) {
          // file exists
          int forkResult = fork();
          if (forkResult < 0) {
            printVerbose("fork failed\n");
            error();
          } else if (forkResult == 0) {
            if (redirectFileName != NULL) {
              int outFd = open(redirectFileName, O_WRONLY|O_APPEND|O_CREAT, 0644);
              if (outFd == -1) {
                printVerbose("Couldn't open/create output file %s\n", redirectFileName);
                printVerbose("%s\n", strerror(errno));
                wasFileError = true;
                error();
                break;
              }
              dup2(outFd, 1);
              dup2(outFd, 2);
              close(outFd);
            }
            if (execv(fname, args) == -1) {
              printVerbose("execv failed\n");
              error();
              exit(1);
            }
          } else {
            int waitResult = wait(NULL);
            if (waitResult == -1) {
              printVerbose("wait failed\n");
              error();
            }
          }
          break; // found the command, so we can stop looking
        } else if (i == *numPathsp - 1) {
          printVerbose("Binary not found\n");
          error();
        }
        free(fname);
      }
      if (wasFileError) {
        break;
      }
    }
    free(args);*/
  } while (lineBuffer != NULL);
}


int main(int argc, char **argv) {
  int numPaths = 0;
  char **paths = NULL;
  reallocPathArr(&paths, &numPaths, 1);
  paths[0] = strdup(initialPath);

  FILE *inStream = stdin;
  bool isInteractiveMode = false;
  if (argc == 1) {
    isInteractiveMode = true;
  } else if (argc == 2) {
    inStream = fopen(argv[1], "r");
    if (inStream == NULL) {
      printVerbose ("Invalid input file\n");
      exitError();
    }
    isInteractiveMode = false;
  } else {
    printVerbose("Wrong number of arguments: %i (should be 0 or 1", argc);
    exitError();
  }

  char *lineBuffer = NULL;
  size_t lineCapacity = 0;
  if (isInteractiveMode) {
    printf("wish> ");
  }
  while (getline(&lineBuffer, &lineCapacity, inStream) != -1) {
    parseLine(lineBuffer, &paths, &numPaths);
    if (isInteractiveMode) {
      printf("wish> ");
    }
  }

  return 0;
}
