#include "kernel/types.h"
#include "user/user.h"

#define N_FORK 20          // fork次数，可自行调大比如50、100
#define MEM_SZ (128*1024)  // 预分配一块内存，用来测试拷贝开销

char buf[MEM_SZ];

int main(void)
{
  int start, end;
  int pid, i;
//   int mem_before, mem_after;

  // 1. 填充内存，让这块内存有数据（原版fork必须完整拷贝这部分）
  for(i = 0; i < MEM_SZ; i++){
    buf[i] = i & 0xff;
  }

  // 采集fork前系统tick、内存占用
  start = uptime();
//   mem_before = getmem(); // 需要你在内核新增sys_getmem，返回当前已使用物理页数

  for(i = 0; i < 1000; i++){
    pid = fork();
    if(pid < 0){
      printf("fork fail\n");
      exit(1);
    }
    if(pid == 0){
      // 子进程：只读内存，不写buf！最大化COW优势
      uint sum = 0;
      for(int j=0;j<MEM_SZ;j++) sum += buf[j];
      exit(0); // 子进程立刻退出
    }else{
      wait(0); // 父进程等待子进程结束
    }
  }

  end = uptime();
//   mem_after = getmem();

  printf("==== Test Result ====\n");
  printf("N_FORK = %d\n", N_FORK);
  printf("Total ticks: %d\n", end - start);
  printf("Avg fork ticks: %d\n", (end-start)/N_FORK);
//   printf("Mem before fork: %d pages\n", mem_before);
//   printf("Mem after all fork+wait: %d pages\n", mem_after);

  exit(0);
}
