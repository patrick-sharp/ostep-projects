#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "pstat.h" // for lottery scheduler
#include "spinlock.h"

// so scheduler can see the process table
extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// new system calls for the lottery scheduler
// set the tickets of the calling process
int 
sys_settickets(void)
{
  int numtickets;
  if(argint(0, &numtickets) < 0)
    return -1;
  myproc()->tickets = numtickets;
  return 0;
}

// get info about a process, including new properties added for this project
int 
sys_getpinfo(void)
{
  struct pstat *pstat_p;
  if(argptr(0, (void *)&pstat_p, sizeof(struct pstat)) < 0)
    return -1;
  if (pstat_p == 0)
    return -1;

  acquire(&ptable.lock);
  for (int i = 0; i < NPROC; i++) {
    if ((pstat_p->inuse[i] = ptable.proc[i].state != UNUSED)){
      pstat_p->tickets[i] = -1;
      pstat_p->pid[i] = -1;
      pstat_p->ticks[i] = -1;
    }
    pstat_p->tickets[i] = ptable.proc[i].tickets;
    pstat_p->pid[i] = ptable.proc[i].pid;
    pstat_p->ticks[i] = ptable.proc[i].ticks;
  }
  release(&ptable.lock);
  return 0;
}
