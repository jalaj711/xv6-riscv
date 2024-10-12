#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"


void
sigalarm(int period, void (*handler)(void)) {
  struct proc * p;
  p = myproc();
  p -> sigalarm_period = period;
  p -> sigalarm_handler = handler;
  p -> sigalarm_tick_counter = 0;
}

void
sigreturn(void) {
  struct proc *p = myproc();
  *(p->trapframe)= *(p -> sigalarm_trapframe);
  p -> sigalarm_is_handler_active = 0;
  p -> sigalarm_tick_counter = 0;
}


uint64
sys_sigalarm(void) {
  int n;
  uint64 p;
  argint(0, &n);
  argaddr(1, &p);

//  void (*fn)(void) = (void *(void)) p;
  sigalarm(n, (void (*)(void) )p);
  return 0;  // not reached
}

uint64
sys_sigreturn(void) {
   sigreturn();
  return 0;  // not reached
}
