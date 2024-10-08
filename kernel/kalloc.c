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
void ksuperfree(void *pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  int is_superpage; // store whether this page is a superpage
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  struct run *superpgfreelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
//
//  struct run *r;
//  acquire(&kmem.lock);
//  r = kmem.freelist;
//  while (r != (struct run *)0x0) {
//    printf("mem init: kmem: %p\n", r);
//    r = r->next;
//  }
//  release(&kmem.lock);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  // if you can make superpages in the free-range
  // then do that greedily
  if (pa_start + SUPERPGSIZE <= pa_end) {
    p = (char *) SUPERPGROUNDUP((uint64)pa_start);
  } else {
    p = (char*) PGROUNDUP((uint64)pa_start);
  }

  for(; p + SUPERPGSIZE <= (char*)pa_end; p += SUPERPGSIZE)
    ksuperfree(p);

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

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  r->is_superpage = 0;
  kmem.freelist = r;
  release(&kmem.lock);
}


// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to ksuperalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
ksuperfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % SUPERPGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("ksuperfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, SUPERPGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.superpgfreelist;
  r->is_superpage = 1;
  kmem.superpgfreelist = r;
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

  // if there is a free superpage then borrow one
  // superpage, split it into normal pages and
  // return the first one
  else if ((r = kmem.superpgfreelist)) {
    kmem.superpgfreelist = r->next;
    for (int i=1;i<512;i++) {
      struct run *t = r-(PGSIZE*i);
      t -> next = kmem.freelist;
      t -> is_superpage = 0;
      kmem.freelist = t;
    }
    r -> is_superpage = 0;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}

// Allocate one 2MB page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
ksuperalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.superpgfreelist;
  if(r)
    kmem.superpgfreelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, SUPERPGSIZE); // fill with junk
  return (void*)r;
}


