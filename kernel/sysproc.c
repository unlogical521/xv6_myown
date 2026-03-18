#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

// Replace your sys_sbrk implementation with the following logic.
// Adjust file/name to where your sys_sbrk actually lives.
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  struct proc* p = myproc();
  addr = p->sz;

if (n > 0) {
  uint64 oldsz = p->sz;
  uint64 lazy_start = PGROUNDUP(oldsz);
  if (p->lazy_start_va == 0 || p->lazy_start_va > lazy_start)
    p->lazy_start_va = lazy_start;
  p->sz += n;
} else if (p->sz + n > 0) {
  uint64 oldsz = p->sz;
  uint64 newsz = oldsz + n;
  p->sz = uvmdealloc(p->pagetable, oldsz, newsz);
  if (p->lazy_start_va != 0) {
    if (PGROUNDUP(p->sz) <= p->lazy_start_va) {
      p->lazy_start_va = 0;
    } else if (p->lazy_start_va < PGROUNDUP(p->sz)) {
      p->lazy_start_va = PGROUNDUP(p->sz);
    }
  }
} else {
    return -1;
  }
  return addr;
}

uint64
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

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
