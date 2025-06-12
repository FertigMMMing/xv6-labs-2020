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

#define NBUCKET 13
#define HASH(id) (id % NBUCKET)

struct hashbuf
{
  /* data */
  struct buf head;
  struct spinlock lock;
};


struct {
  // struct spinlock lock;
  // struct buf buf[NBUF]; // 全局缓冲区的链表结构

  // // Linked list of all buffers, through prev/next.
  // // Sorted by how recently the buffer was used.
  // // head.next is most recent, head.prev is least.
  // struct buf head;
  // 通过hashbuf来代替原本的链表
  struct buf     buf[NBUF];
  struct hashbuf bucket[NBUCKET]; // 实现散列桶
} bcache;

void
binit(void)
{
  struct buf *b;
  char lockname[16];

  for (int i = 0; i < NBUCKET; i++)
  { 
    snprintf(lockname,sizeof(lockname),"bcache_%d",i);
    initlock(&bcache.bucket[i].lock,lockname);
    bcache.bucket[i].head.prev = &bcache.bucket[i].head;
    bcache.bucket[i].head.next = &bcache.bucket[i].head;
  }// 对哈希桶进行初始化

  // 通过头插法将空闲缓存块结构全部挂在哈希桶的第一列
  for(b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->next = bcache.bucket[0].head.next;
    b->prev = &bcache.bucket[0].head;
    bcache.bucket[0].head.next->prev = b;
    bcache.bucket[0].head.next = b;
    initsleeplock(&b->lock,"buffer");
  }


  // initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // 原本的链表结构的实现就不适用
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  
  int bid = HASH(blockno);
  acquire(&bcache.bucket[bid].lock); // 对于缓冲区的锁是直接对于整个结构实现的锁，无法实现多进程的并行操作

  // Is the block already cached?
  for(b = bcache.bucket[bid].head.next; b != &bcache.bucket[bid].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;

      acquire(&tickslock);
      b->timetmp = ticks;
      release(&tickslock);

      release(&bcache.bucket[bid].lock); // 释放当前的哈希桶的锁
      acquiresleep(&b->lock);             // 获取当前缓存块的锁
      return b;
    }
  }

  // Not cached. 当前桶没有该缓存块，需要分配一个区域
  b = 0;
  struct buf* tmp;

  // Recycle the least recently used (LRU) unused buffer.
  for(int i = bid, cycle = 0; cycle != NBUCKET; i = (i + 1) % NBUCKET) // 来实现一圈的访问查询
  {
    ++cycle;

    if(i != bid) // 当前桶肯定不需要了，需要访问别的桶获取空闲块，同时如果别的桶此时已经被获取过锁了
    {
      if(!holding(&bcache.bucket[i].lock))
      {
        acquire(&bcache.bucket[i].lock);
      }
      else  // 如果已经持有锁了，不能再去获得锁，防止死锁；
        continue;
    }

    for(tmp = bcache.bucket[i].head.next; tmp != &bcache.bucket[i].head; tmp = tmp->next)
    {
      //  利用时间戳来进行LRU算法，
      if(tmp->refcnt == 0 && (b == 0 || tmp->timetmp < b->timetmp))
        b = tmp;
    }

    if(b)
    {
      if(i != bid)
      {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.bucket[i].lock);

        b->next = bcache.bucket[bid].head.next;
        b->prev = &bcache.bucket[bid].head;
        bcache.bucket[bid].head.next->prev = b;
        bcache.bucket[bid].head.next = b;

      }
      b->dev = dev;
      b->blockno = blockno;
      b->valid =0;
      b->refcnt = 1;

      acquire(&tickslock);
      b->timetmp = ticks;
      release(&tickslock);

      release(&bcache.bucket[bid].lock);
      acquiresleep(&b->lock);

      return b;
    }

    else{ // b == null
      if(i != bid)
      {
        release(&bcache.bucket[i].lock);
      }
    }
  }

  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
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
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
    
  int bid = HASH(b->blockno);
  releasesleep(&b->lock);

  // acquire(&bcache.lock);
  acquire(&bcache.bucket[bid].lock);

  b->refcnt--;
  // if (b->refcnt == 0) 
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;

  acquire(&tickslock);
  b->timetmp = ticks;
  release(&tickslock);
  
  release(&bcache.bucket[bid].lock);
}

void
bpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.bucket[bid].lock);
  b->refcnt++;
  release(&bcache.bucket[bid].lock);
}

void
bunpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.bucket[bid].lock);
  b->refcnt--;
  release(&bcache.bucket[bid].lock);
}


