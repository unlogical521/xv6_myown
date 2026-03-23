// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
// 写时复制对物理页的释放时机：以引用数作为指标
// 会有很多进程同时在一个物理页上建立映射
// 所有映射消失后，这个物理页才能释放掉
// 解映射时引用减1，建立映射时引用加1
// 64位的内存
// 由pa获取pa索引
#define PA2PAGE_ID(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)
#define PAGE_COUNT     PA2PAGE_ID(PHYSTOP)
// 引用数数组
static int pageref[PAGE_COUNT];
// 共享数组设锁
// 什么时候需要对数组加锁呢？
// 加映射、释放判断
// kalloc呢？
struct spinlock pageref_lock;
#define PAGE2REF(pa) pageref[PA2PAGE_ID((uint64)(pa))]
// 封装一下，便于外部调用
// 定义函数
void pageRef(void* pa){
  acquire(&pageref_lock);
  PAGE2REF(pa)++;
  release(&pageref_lock);
}
void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pageref_lock,"pageref");
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
// 修改这里
// 如果引用计数归0 则释放掉这个物理页
void
kfree(void *pa)
{  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  acquire(&pageref_lock);
  // 每次调用 kfree，都减少一次引用
  if(--PAGE2REF(pa) <= 0){
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&pageref_lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);
  
  if(r)
{    memset((char*)r, 5, PGSIZE); // fill with junk
    // 物理页分配时，引用数设为1
    PAGE2REF(r)=1;}
  return (void*)r;
}
// 写时复制分配的页
void* cow_kalloc(void* pa){
  //判断旧页的引用数是否为1
  void* new_pa;
  acquire(&pageref_lock);
  //判断引用数
  //为1则返回旧页地址
  if(PAGE2REF(pa) <= 1){
    release(&pageref_lock);
    return pa;
  }
  if((new_pa = kalloc()) == 0){
    release(&pageref_lock);
    return 0;
  }
  // 复制数据
  memmove(new_pa,pa,PGSIZE);
  PAGE2REF(pa)--;
  release(&pageref_lock);
  return new_pa;
}