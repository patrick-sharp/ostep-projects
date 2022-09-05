#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"

// width must be <= 15
int
print_width_int(int width, int n) {
  static char digits[] = "0123456789";
  char buf[16] = { [0 ... 15 ] = ' ' }; // gcc designated initializer
  int temp;
  int int_width;
  int i;

  if (n < 0)
    return -1;
   
  temp = n;
  int_width = 0;
  do {
    int_width++;
  } while ((temp /= 10) != 0);

  if (int_width > width)
    return -1;

  i = width - 1;
  do {
    buf[i--] = digits[n % 10];
    if (i < 0)
      return -1;
  } while((n /= 10) != 0);
  buf[width] = ' ';
  buf[width + 1] = '\0';

  printf(1, buf);
  return 0;
}

int
main(int argc, char *argv[])
{
  struct pstat pinfo;
  int i;
  if (getpinfo(&pinfo) == -1) {
    printf(1, "ERROR: getpinfo returned -1\n");
    exit();
  }
  printf(1, "pid tickets   ticks\n");
  for (i = 0; i < NPROC; i++) {
    if (!pinfo.inuse[i])
      continue;
    print_width_int(3, pinfo.pid[i]);
    print_width_int(7, pinfo.tickets[i]);
    print_width_int(7, pinfo.ticks[i]);
    printf(1, "\n");
  }
  exit();
}       
