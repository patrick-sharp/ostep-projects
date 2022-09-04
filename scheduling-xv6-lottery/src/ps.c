#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"

int
main(int argc, char *argv[])
{
  //struct pstat pinfo;
  printf(1, "About to getpinfo\n");
  printf(1, "%d\n", uptime());
  //getpinfo(&pinfo);
  printf(1, "uptime addr %p\n", uptime);
  printf(1, "read addr %p\n", read);
  printf(1, "wait addr %p\n", wait);
  printf(1, "getpid addr %p\n", getpid);
  printf(1, "fork addr %p\n", fork);
  printf(1, "close addr %p\n", close);
  printf(1, "getpinfo addr %p\n", getpinfo);
  printf(1, "getpinfo'ed\n");
  exit();
}
