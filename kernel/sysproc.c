#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

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
// trace系统调用
//获取参数
uint64
sys_trace(void){
  int mask;
  //用户进程调用trace函数，系统调用号为[SYS_trace]
  //trace这个系统调用的参数是a0~a6这些寄存器的内容
  //a0内容赋值给mask
  if(argint(0,&mask) < 0){
    return -1;
  }
  //进程只有在调用trace时，其sysmask_trace才会生效
  myproc()->sysmask_trace = mask;
  return 0;
}
//获取系统的空闲内存字节数和非运行进程数
uint64
sys_sysinfo(void){
  struct sysinfo si;
  //指针比引用方便些
  //直接在目标地址写入
  unusedpro_num(&si.nproc);
  kfree_mem_size(&si.freemem);
  //获取参数
  //获取地址
  uint64 dstaddr;
  //argaddr的作用是将地址参数写入指针
  //*ip = a0;
  //指针变量ip记录着a0的值
  if(argaddr(0,&dstaddr) < 0){
    return -1;
  }
  //dstaddr是个地址
  if(copyout(myproc()->pagetable,dstaddr,(char*)&si,sizeof(si))<0){
    return -1;
  }
  return 0;
}