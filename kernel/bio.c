// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
// 13个散列桶
// 13个链表
// 将高速缓冲区切分为 13 块
#define NBUCKET 13
struct {
  
  struct buf buf[NBUF];
  // 散列桶
  struct buf bucket[NBUCKET];
  struct spinlock bucket_lock[NBUCKET];
  struct spinlock eviction_lock;
  // // Linked list of all buffers, through prev/next.
  // // Sorted by how recently the buffer was used.
  // // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

char* bcache_lock_name[]={
  "bcache_lock_0",
  "bcache_lock_1",
  "bcache_lock_2",
  "bcache_lock_3",
  "bcache_lock_4",
  "bcache_lock_5",
  "bcache_lock_6",
  "bcache_lock_7",
  "bcache_lock_8",
  "bcache_lock_9",
  "bcache_lock_10",
  "bcache_lock_11",
  "bcache_lock_12",
};
void
binit(void)
{
  struct buf *b;
  // 先初始化
  // 最开始怎么分配呢？
  // 先分到一个桶里
  // 然后在bget设计偷块
  initlock(&bcache.eviction_lock,"eviction_lock");
  for(int i=0;i<NBUCKET;i++){
    // 初始化桶锁
    initlock(&bcache.bucket_lock[i], bcache_lock_name[i]);
    bcache.bucket[i].prev = &bcache.bucket[i];
    bcache.bucket[i].next = &bcache.bucket[i];
  }
  // Create linked list of buffers
  // 建立环形链表
  for(int i=0;i<NBUF;i++){
    b = &bcache.buf[i];
    b->next = bcache.bucket[0].next;
    bcache.bucket[0].next->prev = b;
    b->prev = &bcache.bucket[0];
    bcache.bucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // 怎么确定唯一的键？
  // blockno，dev太少了
  int n = blockno % NBUCKET;
  // 锁定桶
  acquire(&bcache.bucket_lock[n]);
  // Is the block already cached?

  for(b = bcache.bucket[n].next; b != &bcache.bucket[n]; b = b->next){
    // 这里找块找的是特定(dev和blockno)的块
    // 可以以此作为键？设计散列桶。
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[n]);
      // 块的休眠锁，需要等上一个进程释放才能获取
      acquiresleep(&b->lock);
      return b;
    }
  }
  // 缓存未命中
  // // 先遍历自己的桶
  // // Not cached.
  // // Recycle the least recently used (LRU) unused buffer.
  // // 这里是在找最近最少使用的块，重置它
  // for(b = bcache.bucket[n].prev; b != &bcache.bucket[n]; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     // 用于判断是否可用，重置后需要重新从磁盘读取数据
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.bucket[n].lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  // 偷块操作
  // 偷什么样的？
  // 只能偷最近未使用的？
  // 怎么样的才算是最近未使用的呢？
  // 链尾的 并且引用次数为0的，是LRU
  // 遍历所有桶？
  // 遍历所有桶
  // 为避免死锁,应先释放掉当前桶锁
  release(&bcache.bucket_lock[n]);
  // 但这又有一个问题，释放这个锁后，当前桶可能已经偷到了？
  // 这会导致什么后果呢？
  // 同一个磁盘块映射了多次，数据读写乱套
  // 为避免这种情况
  // 偷块必须是单线程，必须加锁，但又不能是桶锁，只能是其他锁，直到偷完了
  acquire(&bcache.eviction_lock);
  // 判断加锁期间有没有偷到
  // 重新加上锁
  acquire(&bcache.bucket_lock[n]);
  for(b = bcache.bucket[n].next; b != &bcache.bucket[n]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[n]);
      release(&bcache.eviction_lock);
      // 块的休眠锁，需要等上一个进程释放才能获取
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_lock[n]);
  // 没有偷到，就偷
  int find = 0;
  struct buf * stolen_buf;
  for(int i=0;i<NBUCKET;i++){
    acquire(&bcache.bucket_lock[i]);
    // 判断有没有LRU
    // 有块 && 有LRU
    struct buf * bb = &bcache.bucket[i];
    stolen_buf = bb->prev;
    if(stolen_buf != bb &&  stolen_buf->refcnt == 0){
      // 把这块单独拎出来
      find = 1;
      stolen_buf->prev->next = bb;
      bb->prev = stolen_buf->prev;
      release(&bcache.bucket_lock[i]);
      break;
    }
    release(&bcache.bucket_lock[i]);
  }
  if(find == 0){
    panic("bget: no buffers");
  }
  // 塞进桶里
  acquire(&bcache.bucket_lock[n]);
  // 添加到最前面
  bcache.bucket[n].next->prev = stolen_buf;
  stolen_buf->next = bcache.bucket[n].next;
  bcache.bucket[n].next = stolen_buf;
  stolen_buf->prev = &bcache.bucket[n];
  stolen_buf->dev = dev;
  stolen_buf->blockno = blockno;
  stolen_buf->refcnt = 1;
  stolen_buf->valid = 0;
  release(&bcache.bucket_lock[n]);
  release(&bcache.eviction_lock);
  acquiresleep(&stolen_buf->lock);
  return stolen_buf;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  // 这里从磁盘写入
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  // 写出时需持有块锁
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  // 写进磁盘
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  // 释放时，也需持有块锁
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int n = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[n]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = &bcache.bucket[n];
    b->prev = bcache.bucket[n].prev;
    bcache.bucket[n].prev->next = b;
    bcache.bucket[n].prev = b;
  }
  
  release(&bcache.bucket_lock[n]);
}

void
bpin(struct buf *b) {
  int n = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[n]);
  b->refcnt++;
  release(&bcache.bucket_lock[n]);
}

void
bunpin(struct buf *b) {
  int n = b->blockno % NBUCKET;
  acquire(&bcache.bucket_lock[n]);
  b->refcnt--;
  release(&bcache.bucket_lock[n]);
}


