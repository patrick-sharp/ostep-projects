#include "types.h"
#include "stat.h"
#include "user.h"

#define NULL 0

#define LEN 40

// printf has been modified to be able to print out null.

int
main(int argc, char *argv[])
{
  char *blah = NULL;
  printf(1, "value at null: %s\n", blah);
  exit();
  //char buf[LEN];
  //mprotect(buf, LEN);
  //munprotect(buf, LEN);
  //int parent = getpid();
}
