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

typedef struct {
  struct spinlock lock;
  struct run *freelist;
  int *ref_count;
  void * pa_start;
} kmem_t;

kmem_t cpu_kmem[NCPU];

void
kinit()
{
  for(int i=0;i<NCPU;i++){
    char name[6] = "kmem_ ";
    name[5] = '0' + i;
    initlock(&cpu_kmem[i].lock, name);
  }
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

  // freerange is only called by kinit which is only called once
  // during boot, by main.c. This is only called on the cpu id 0
  // so we can directly hardcode [0]
  cpu_kmem[0].ref_count = (int *)p;
  // we reserve the first few pages for reference count storage
  // so the actual usable physical ram starts from p+sz;
  cpu_kmem[0].pa_start = p+sz;
  p = cpu_kmem[0].pa_start;
  for(; p + PGSIZE <= pa_end; p += PGSIZE) {
    cpu_kmem[0].ref_count[((char *)p - (char *)cpu_kmem[0].pa_start)/PGSIZE] = 1;
    kfree(p);
  }

  // copy the physical address start and ref_count pointer to each CPU
  // because these will be same for each CPU throughout the program
  for(int i=1;i<NCPU;i++) {
    cpu_kmem[i].pa_start = cpu_kmem[0].pa_start;
    cpu_kmem[i].ref_count = cpu_kmem[0].ref_count;
  }
}

// function to add 'n' to the ref count of physical address pa
// panics if final ref count goes below 0
// frees the page if ref count becomes zero
void
krefcntadd(void *pa, int n) {
  struct run *r;
  const int cpu_id = cpuid();
  char * pa_start;
  pa_start = (char *)cpu_kmem[cpu_id].pa_start;

  if(((uint64)pa % PGSIZE) != 0 || (char *)pa < pa_start || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&cpu_kmem[cpu_id].lock);
  int *ref = &cpu_kmem[cpu_id].ref_count[((char *)pa - pa_start)/PGSIZE];
  *ref += n;

  if (*ref < 0) {
    panic("krefcntadd");
  }

  if (*ref == 0) {
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = cpu_kmem[cpu_id].freelist;
    cpu_kmem[cpu_id].freelist = r;
  }
  release(&cpu_kmem[cpu_id].lock);
}

// returns the ref count of the page at pa
int
krefcnt(void *pa) {
  char * pa_start;
  const int cpu_id = cpuid();
  pa_start = (char *)cpu_kmem[cpu_id].pa_start;

  if(((uint64)pa % PGSIZE) != 0 || (char *)pa < pa_start || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&cpu_kmem[cpu_id].lock);
  int ref = cpu_kmem[cpu_id].ref_count[((char *)pa - pa_start)/PGSIZE];
  release(&cpu_kmem[cpu_id].lock);

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

// function to steal the memory from another cpu
// when this current CPU's memory goes empty
struct run *
stealmem()
{
  struct run *r;
  int flag = 0;
  for (int i=0;i<NCPU;i++) {
    acquire(&cpu_kmem[i].lock);
    r = cpu_kmem[i].freelist;
    if (r) {
      cpu_kmem[i].freelist = r->next;
      ++cpu_kmem[i].ref_count[((char *)r - (char *)cpu_kmem[i].pa_start) / PGSIZE];
      flag = 1;
    }
    release(&cpu_kmem[i].lock);
    if (flag) break;
  }
  return r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  const int cpu_id = cpuid();

  acquire(&cpu_kmem[cpu_id].lock);
  r = cpu_kmem[cpu_id].freelist;
  if(r){
    cpu_kmem[cpu_id].freelist = r->next;
    // increment ref count
    ++cpu_kmem[cpu_id].ref_count[((char *)r - (char *)cpu_kmem[cpu_id].pa_start) / PGSIZE];
  }
  release(&cpu_kmem[cpu_id].lock);

  // if our memory is not free then try stealing from some
  // other cpu. It is important to release our own lock before
  // trying to steal memory because if we don't then multiple
  // CPUs trying to steal memory together can easily lead to
  // a deadlock.
  if(r || (r = stealmem()))
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}
