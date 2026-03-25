// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};
// 每个cpu一个空闲链表
// 空闲链表数组
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];
//
// 每把锁都有一个名字
char* kmem_lock_names[] = {
    "kmem_cpu_0",
    "kmem_cpu_1",
    "kmem_cpu_2",
    "kmem_cpu_3",
    "kmem_cpu_4",
    "kmem_cpu_5",
    "kmem_cpu_6",
    "kmem_cpu_7",
};
void
kinit()
{
  // 初始化每个cpu的 freelists 锁
  for(int i=0;i<NCPU;i++){
    initlock(&kmem[i].lock,kmem_lock_names[i]);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  // 关中断
  push_off();
  int i = cpuid();
  acquire(&kmem[i].lock);
  r->next = kmem[i].freelist;
  kmem[i].freelist = r;
  release(&kmem[i].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  // 分配时
  // 关中断
  push_off();
  int cpu = cpuid();
  // 获取对应链表锁
  acquire(&kmem[cpu].lock);
  // 如果当前cpu没内存了，需要从其它cpu里偷
  if(!kmem[cpu].freelist){
    // 偷多少呢？
    int steal_num = 64;
    for(int i=0;i<NCPU;i++){
      //跳过自己
      if(i==cpu)continue;
      //偷其他的
      //获取锁
      acquire(&kmem[i].lock);
      struct run * temp = kmem[i].freelist;
      while(kmem[i].freelist && steal_num--){
        
        kmem[i].freelist = temp->next;
        temp->next = kmem[cpu].freelist;
        kmem[cpu].freelist = temp;
        temp = kmem[i].freelist;
      }
      release(&kmem[i].lock);
      // 偷完后就直接结束遍历
      if(steal_num == 0){
        break;
      }
    }
  }
  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  release(&kmem[cpu].lock);
  pop_off();
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
