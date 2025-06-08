// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
// 定义Cow的宏
#define PA2PGREF_ID(p) (((p)-KERNBASE)/PGSIZE) //通过物理地址来获取当前页号
#define PGREF_MAX_ENTRIES PA2PGREF_ID(PHYSTOP) // 物理页的上限

int pageref[PGREF_MAX_ENTRIES]; // 对于每个页号的引用的保存
struct spinlock pgreflock;// 在数组添减引用的时候，防止竞态条件产生的错误

#define PA2PGREF(p) pageref[PA2PGREF_ID((uint64)(p))] //获得当前地址对应的引用数目




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
  initlock(&pgreflock, "pgreflock");
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
  acquire(&pgreflock);
  if(--PA2PGREF(pa) <= 0)
  {
    memset(pa, 1, PGSIZE);
    
    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&pgreflock);
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
  {
    memset((char*)r, 5, PGSIZE); // fill with junk
    PA2PGREF(r) = 1;  
  }
  return (void*)r;
}


void* _kcopy_def(void* pa)// 对于一个重引用的物理地址的解引用
{
  acquire(&pgreflock);

  if(PA2PGREF(pa) <= 1)
  {
    release(&pgreflock);
    return pa;
  }

  uint64 newpa = (uint64)kalloc();
  if(newpa == 0)
  {
    release(&pgreflock);
    return 0;
  }
  memmove((void*)newpa,(void*)pa,PGSIZE); // 将当前页的内容复制过去
  
  PA2PGREF(pa)--;
  release(&pgreflock);
  
  return (void*)newpa;
}


void krefpage(void* pa) // 对于一个物理地址的重引用
{
  acquire(&pgreflock);
  PA2PGREF(pa)++;
  release(&pgreflock);
}