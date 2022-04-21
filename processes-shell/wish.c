#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
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

// global state
char **globalPaths = NULL;
int globalNumPaths = 0;

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
  error();
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

void freePathArr() {
  for (int i = 0; i < globalNumPaths; i++) {
    free(globalPaths[i]);
  }
  free(globalPaths);
}

void reallocPathArr(int newNumPaths) {
  if (globalPaths != NULL) {
    freePathArr();
  }
  globalPaths = (char **) malloc(sizeof(char **) * newNumPaths);
  if (globalPaths == NULL) {
    printVerbose("malloc failed in reallocPathArr\n");
    error();
  }
  globalNumPaths = newNumPaths;
}

void *parseParallelCommand(void *parallelCommandVoid) {
  char *parallelCommand = (char *) parallelCommandVoid;
  char *redirectFileName = NULL;
  int numArgs = getNumArgs(parallelCommand, &redirectFileName);
  printVerbose("redirect file name: %s\n", redirectFileName);
  if (numArgs == -1) {
    return NULL;
  } else if (numArgs == 0) {
    if (redirectFileName != NULL) {
      printVerbose("must supply a command to redirect\n");
      error();
    }
    return NULL;
  }
  char **args = parseArgs(&parallelCommand, numArgs);
  if (strcmp(args[0], "cd") == 0) {
    if (!assertArgs("cd", 2, numArgs)) {
      return NULL;
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
      return NULL;
    }
    printVerbose("exit\n");
    exit(0);
  } else if (strcmp(args[0], "path") == 0) {
    reallocPathArr(numArgs - 1);
    //printVerbose("# paths: %i, paths: ", globalNumPaths);
    for (int i = 1; i < numArgs; i++) {
      globalPaths[i-1] = strdup(args[i]);
    }
    printVerbose("\n");
  } else {
    if (globalNumPaths == 0) {
      printVerbose("No path - cannot find non-builtin command\n");
      error();
      return NULL;
    }
    for (int i = 0; i < globalNumPaths; i++) {
      char *fname = (char *) malloc(strlen(globalPaths[i]) + 1 + strlen(args[0]) + 1);
      if (fname == NULL) {
        printVerbose("malloc failed for fname\n");
        error();
      }
      strcpy(fname, globalPaths[i]);
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
            int outFd = open(redirectFileName, O_WRONLY|O_CREAT, 0644);
            if (outFd == -1) {
              printVerbose("Couldn't open/create output file %s\n", redirectFileName);
              printVerbose("%s\n", strerror(errno));
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
      } else if (i == globalNumPaths - 1) {
        printVerbose("Binary not found\n");
        error();
      }
      free(fname);
    }
  }
  free(args);
  return NULL;
}

void parseLine(char *lineBuffer) {
  // figure out how many parallel commands there are
  int numCommands = 1;
  int i = 0;
  while (lineBuffer[i] != '\0') {
    if (lineBuffer[i] == '&') {
      numCommands += 1;
    }
    i += 1;
  }

  // make the threads
  pthread_t *threads = (pthread_t *) malloc(sizeof(pthread_t) * numCommands);
  for (int i = 0; i < numCommands; i++) {
    char *parallelCommand = strsep(&lineBuffer, "&");
    int rc = pthread_create(&threads[i], NULL, &parseParallelCommand, (void *) parallelCommand);
    if (rc) {
      printVerbose("Error creating thread %i, command: %s\n", i, parallelCommand);
      exitError();
    }
  }

  // join the threads
  for (int i = 0; i < numCommands; i++) {
    int thread_rc = pthread_join(threads[i], NULL);
    if (thread_rc) {
      printVerbose("Error in thread %i\n");
      error();
    }
  }
  free(threads);
}


int main(int argc, char **argv) {
  reallocPathArr(1);
  globalPaths[0] = strdup(initialPath);

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
    parseLine(lineBuffer);
    if (isInteractiveMode) {
      printf("wish> ");
    }
  }

  return 0;
}
