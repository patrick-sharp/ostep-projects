#include "types.h"
#include "stat.h"
#include "user.h"

#define ITERS 1000

void print_letter(char c) {
  for (int i = 0; i < ITERS; i++) {
    if (i % 50 == 0) {
      printf(1, "%c\n", c);
    } else {
      printf(1, "%c", c);
    }
  }
}

int main(int argc, char *argv[])
{
  int childpid;
  int grandchildpid;
  settickets(10);
  if ((childpid = fork())) {
    // parent
    print_letter('a');
    wait();
    printf(1, "\n");
  } else {
    // child
    settickets(20);
    if ((grandchildpid = fork())) {
      // child
      print_letter('b');
      wait();
    } else {
      // grandchild
      settickets(50);
      print_letter('c');
    }
  }
  exit();
}
