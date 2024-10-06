
#include "kernel/types.h"
#include "user/user.h"


void
ugetpid_test()
{
  int i;

  printf("ugetpid_test starting\n");

  for (i = 0; i < 64; i++) {
    int ret = fork();
    if (ret != 0) {
      wait(&ret);
      if (ret != 0)
        exit(1);
      continue;
    }
    if (getpid() != ugetpid())
      printf("missmatched PID");
    exit(0);
  }
  printf("ugetpid_test: OK\n");
}

int main(int argc, char * argv) {
    ugetpid_test();
    return 0;
}