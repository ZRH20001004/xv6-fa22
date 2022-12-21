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
} kmem[NCPU];

void
get_cpuid(int *id)
{
  push_off();
  *id = cpuid();
  pop_off();
}

void
kinit()
{
  //initlock(&kmem.lock, "kmem");
  for(int i = 0; i < NCPU; i++)
  {
    //cpu id
    int cpu;
    get_cpuid(&cpu);
    if(cpu < 0 || cpu >= NCPU)
    {
      printf("kinit:cpuid %d is not true.\n", cpu);
    }
    initlock(&kmem[cpu].lock, "kmem");
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

  int cpu;
  get_cpuid(&cpu);

  if(cpu < 0 || cpu >= NCPU)
  {
    printf("kfree:cpuid %d is not true.\n", cpu);
  }

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
}

void *
get_free(int id)
{
  int i;
  struct run *r;
  for(i = 0; i < NCPU; i++)
  {
    if(i != id)
    {
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r)
      {
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        return r;
      }
      release(&kmem[i].lock);
    }
  }
  return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  int cpu;
  get_cpuid(&cpu);

  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  if(r)
  {
    kmem[cpu].freelist = r->next;
  } else {
    r = get_free(cpu);
  }
  release(&kmem[cpu].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
