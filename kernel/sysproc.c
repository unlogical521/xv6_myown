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

uint64
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

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;
  backtrace();
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
// 软件中断
// 这个系统调用，跳转到进程的处理函数那执行
// 重要的是理解陷入、处理和返回时寄存器和程序计数器的变化
uint64
sys_sigalarm(void){
  //倒计时
  int ticks;
  uint64 handler;
  if(argint(0,&ticks) < 0){
    return -1;
  }
  if(argaddr(1,&handler) < 0){
    return -1;
  }
  return sigalarm(ticks,(void(*)())handler);
  // 这个系统调用的作用？
  // 跳到处理函数位置
  // 在此之前需要保存寄存器状态
  // 陷入时，已将进程状态保存到trapframe
  // 中断结束后，自动返回，
  // 因此当满足软件中断条件时，只需要将trapframe中的程序计数器的值改为handler函数的位置
}
// 这个系统调用触发，又使得trapframe面目全非
// 需要设计一个副本用于保存特定时期进程的状态
// 这个则返回sigalarm调用的下一条指令继续执行
// 前提恢复寄存器状态
// 恢复谁的呢？
// 恢复到调用sigalarm时保存的状态
// 需要完全恢复trapframe的值并且epc+4;
uint64
sys_sigreturn(void){
  return sigreturn();
}