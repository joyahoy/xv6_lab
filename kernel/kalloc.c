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

#define PA2IDX(pa) (((uint64)(pa)) >> 12)  // 将物理地址转换为数组索引

struct {
  struct spinlock lock;
  int count[(PHYSTOP - KERNBASE) / PGSIZE]; // 引用计数数组
} refcount;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// 初始化引用计数（在 kinit() 中调用）
void refcount_init() {
  initlock(&refcount.lock, "refcount");
  //memset(refcount.count, 0, sizeof(refcount.count));
}

void kinit()
{
  initlock(&kmem.lock, "kmem");
  refcount_init(); // 先初始化引用计数
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    refcount.count[PA2IDX((uint64)p)] = 1; // boot 阶段初始化
    kfree(p);
  }
}

// 增加引用计数
void incref(uint64 pa) {
  acquire(&refcount.lock);
  if(pa >= PHYSTOP)
    panic("incref: pa too high");
  if(pa % PGSIZE != 0)
    panic("incref: pa not aligned");
  refcount.count[PA2IDX(pa)]++;
  release(&refcount.lock);
}

// 减少引用计数，返回是否应该释放
int decref(uint64 pa) {
  acquire(&refcount.lock);
  if(pa >= PHYSTOP)
    panic("decref: pa too high");
  if(pa % PGSIZE != 0)
    panic("decref: pa not aligned");
  if(refcount.count[PA2IDX(pa)] < 1)
    panic("decref: zero refcount");
  refcount.count[PA2IDX(pa)]--;
  int should_free = (refcount.count[PA2IDX(pa)] == 0);
  release(&refcount.lock);
  return should_free;
}

// 获取物理页的引用计数
int getref(uint64 pa) {
    acquire(&refcount.lock);
    int count = refcount.count[PA2IDX(pa)];
    release(&refcount.lock);
    return count;
}

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    // 初始化引用计数为1
    acquire(&refcount.lock);
    refcount.count[PA2IDX((uint64)r)] = 1;
    release(&refcount.lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(decref((uint64)pa)) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
}
/*
// Free the page of physical memory pointed at by pa,
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

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
*/