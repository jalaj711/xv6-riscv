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
  int *ref_count;
  void * pa_start;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  void *p;
  p = (void*)PGROUNDUP((uint64)pa_start);

  // this much size is required to store ref count of all pages
  int sz = (pa_end - p) / PGSIZE  * sizeof(int);
  sz = PGROUNDUP(sz);

  kmem.ref_count = (int *)p;
  // we reserve the first few pages for reference count storage
  // so the actual usable physical ram starts from p+sz;
  kmem.pa_start = p+sz;
  p = kmem.pa_start;
  for(; p + PGSIZE <= pa_end; p += PGSIZE) {
    kmem.ref_count[((char *)p - (char *)kmem.pa_start)/PGSIZE] = 1;
    kfree(p);
  }
}

// function to add 'n' to the ref count of physical address pa
// panics if final ref count goes below 0
// frees the page if ref count becomes zero
void
krefcntadd(void *pa, int n) {
  struct run *r;
  char * pa_start;
  pa_start = (char *)kmem.pa_start;

  if(((uint64)pa % PGSIZE) != 0 || (char *)pa < pa_start || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  int *ref = &kmem.ref_count[((char *)pa - pa_start)/PGSIZE];
  *ref += n;

  if (*ref < 0) {
    panic("krefcntadd");
  }

  if (*ref == 0) {
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kmem.lock);
}

// returns the ref count of the page at pa
int
krefcnt(void *pa) {
  char * pa_start;
  pa_start = (char *)kmem.pa_start;

  if(((uint64)pa % PGSIZE) != 0 || (char *)pa < pa_start || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  int ref = kmem.ref_count[((char *)pa - pa_start)/PGSIZE];
  release(&kmem.lock);

  return ref;
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  // the function will handle whether to actually free the mem
  krefcntadd(pa, -1);
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
  if(r){
    kmem.freelist = r->next;
    // increment ref count
    ++kmem.ref_count[((char *)r - (char *)kmem.pa_start) / PGSIZE];
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
