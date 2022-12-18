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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock[(PHYSTOP - KERNBASE) / PGSIZE]; 
  uint ref_count[(PHYSTOP - KERNBASE) / PGSIZE];
} ref;

int
get_idx(uint64 pa)
{
  return (pa - KERNBASE) / PGSIZE;
}
//reduce the ref count respond to pa.
void
reduce(uint64 pa)
{
  int i = get_idx(pa);
  acquire(&ref.lock[i]);
  ref.ref_count[i]--;
  release(&ref.lock[i]);
}

//add the ref count respond to pa.
void
add(uint64 pa)
{
  int i = get_idx(pa);
  acquire(&ref.lock[i]);
  ref.ref_count[i]++;
  release(&ref.lock[i]);
}

// uint
// get_count(uint64 pa)
// {
//   int count;
//   int i = get_idx(pa);
//   acquire(&ref.lock[i]);
//   count = ref.ref_count[i];
//   release(&ref.lock[i]);
//   return count;
// }

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  for(int i = 0; i < (PHYSTOP - KERNBASE) / PGSIZE; i++)
  {
    initlock(&ref.lock[i], "ref");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    add((uint64)p);
    kfree(p);
  }
}

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

  int i = get_idx((uint64)pa);
  acquire(&ref.lock[i]);
  if(ref.ref_count[i] == 1)
  {
    ref.ref_count[i] = 0;
    release(&ref.lock[i]);
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  } else {
    ref.ref_count[i]--;
    release(&ref.lock[i]);
  }
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
    int i = get_idx((uint64)r);
    acquire(&ref.lock[i]);
    ref.ref_count[i] = 1;
    release(&ref.lock[i]);
  }
  return (void*)r;
}
