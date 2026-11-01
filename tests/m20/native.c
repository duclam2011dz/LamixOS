/* SPDX-License-Identifier: GPL-3.0-only */
#include "process.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FRAMES 128u
typedef struct frame { uint64_t physical; uint8_t bytes[VMM_PAGE_SIZE]; int used; } frame;
typedef struct machine { frame frames[TEST_FRAMES]; uint64_t next; int allocation_budget; } machine;
static frame *find(machine *m, uint64_t physical) {
  for (unsigned i=0;i<TEST_FRAMES;i++)
    if (m->frames[i].used && m->frames[i].physical==physical) return &m->frames[i];
  return 0;
}
static int allocate(void *context, uint64_t *physical) {
  machine *m=context;
  if(m->allocation_budget==0)return 0;
  if(m->allocation_budget>0)--m->allocation_budget;
  for(unsigned i=0;i<TEST_FRAMES;i++) if(!m->frames[i].used) {
    m->frames[i].used=1; m->frames[i].physical=m->next; m->next+=VMM_PAGE_SIZE;
    memset(m->frames[i].bytes,0,sizeof(m->frames[i].bytes)); *physical=m->frames[i].physical; return 1;
  }
  return 0;
}
static int release(void *context,uint64_t physical) { frame *f=find(context,physical); if(!f)return 0; f->used=0; return 1; }
static volatile uint8_t *access_frame(void *context,uint64_t physical) { frame *f=find(context,physical); return f?f->bytes:0; }
static volatile uint64_t *access_table(void *context,uint64_t physical) { return (volatile uint64_t *)access_frame(context,physical); }
static void invalidate(void *context,uint64_t address) { (void)context; (void)address; }
static void *heap_alloc(void *context,size_t size) { (void)context; return calloc(1,size); }
static void heap_free(void *context,void *pointer) { (void)context; free(pointer); }
static uintptr_t irq_save(void *context) { (void)context; return 0; }
static void irq_restore(void *context,uintptr_t flags) { (void)context; (void)flags; }
static int reschedule(void *context) { (void)context; return 0; }
static void fatal(void *context,uint64_t reason) { (void)context; (void)reason; abort(); }
static void noop(void *argument) { (void)argument; }
static unsigned used_frames(machine *m) { unsigned n=0; for(unsigned i=0;i<TEST_FRAMES;i++)n+=m->frames[i].used!=0; return n; }

int main(void) {
  machine m={.next=0x100000,.allocation_budget=-1};
  vmm_operations vmops={&m,allocate,release,access_table,invalidate};
  vmm_space kernel={0};
  assert(vmm_init(&kernel,&vmops,1)==VMM_OK);
  uint64_t kernel_page; assert(allocate(&m,&kernel_page));
  const uint64_t kernel_va=UINT64_C(0xffffffff80000000);
  assert(vmm_map_range(&kernel,kernel_va,kernel_page,VMM_PAGE_SIZE,VMM_READ|VMM_EXECUTE,0)==VMM_OK);
  assert(vmm_map_range(&kernel,UINT64_C(0x00300000),kernel_page,VMM_PAGE_SIZE,
                       VMM_READ|VMM_WRITE,0)==VMM_OK);
  scheduler sched; scheduler_operations schedops={&m,heap_alloc,heap_free,irq_save,irq_restore,reschedule,fatal,0,0,0};
  assert(scheduler_init(&sched,&schedops,noop)==SCHED_OK);
  process_operations ops={.context=&m,.allocate_frame=allocate,.release_frame=release,.access_frame=access_frame,.vmm=vmops};
  const uint8_t payload[]={0x90,0xeb,0xfe}; process a={0},b={0};
  unsigned baseline=used_frames(&m);
  m.allocation_budget=3;
  assert(process_create(&a,&ops,&kernel,&sched,9,payload,sizeof(payload),2)==PROCESS_OUT_OF_MEMORY);
  assert(used_frames(&m)==baseline);
  m.allocation_budget=-1;
  vmm_space no_nx=kernel; no_nx.nx_enabled=0;
  assert(process_create(&a,&ops,&no_nx,&sched,9,payload,sizeof(payload),2)==PROCESS_INVALID);
  assert(process_create(&a,&ops,&kernel,&sched,1,payload,sizeof(payload),2)==PROCESS_OK);
  assert(process_create(&b,&ops,&kernel,&sched,2,payload,sizeof(payload),2)==PROCESS_OK);
  assert(vmm_map_range(&b.address_space,PROCESS_USER_PRIVATE,b.data_frame,
                       VMM_PAGE_SIZE,VMM_READ|VMM_WRITE|VMM_USER,0)==VMM_OK);
  uint64_t pa=0,pb=0,flags=0; uint32_t perms=0;
  assert(vmm_query(&a.address_space,PROCESS_USER_CODE,&pa,&perms,&flags)==VMM_OK);
  assert((perms&(VMM_USER|VMM_EXECUTE))==(VMM_USER|VMM_EXECUTE) && !(perms&VMM_WRITE));
  assert(vmm_query(&a.address_space,PROCESS_USER_DATA,&pa,&perms,&flags)==VMM_OK);
  assert((perms&(VMM_USER|VMM_WRITE))==(VMM_USER|VMM_WRITE) && !(perms&VMM_EXECUTE));
  assert(vmm_query(&b.address_space,PROCESS_USER_DATA,&pb,&perms,&flags)==VMM_OK);
  assert(pa!=pb);
  assert(vmm_query(&a.address_space,kernel_va,&pa,&perms,&flags)==VMM_OK && !(perms&VMM_USER));
  assert(vmm_query(&a.address_space,UINT64_C(0x00300000),&pa,&perms,&flags)==VMM_OK && !(perms&VMM_USER));
  assert(vmm_query(&a.address_space,PROCESS_USER_GUARD,&pa,&perms,&flags)==VMM_NOT_MAPPED);
  assert(vmm_query(&a.address_space,PROCESS_USER_INVALID,&pa,&perms,&flags)==VMM_NOT_MAPPED);
  assert(vmm_query(&a.address_space,PROCESS_USER_PRIVATE,&pa,&perms,&flags)==VMM_NOT_MAPPED);
  assert(vmm_query(&b.address_space,PROCESS_USER_PRIVATE,&pa,&perms,&flags)==VMM_OK);
  assert((pa&~(VMM_PAGE_SIZE-1))==b.data_frame && (perms&VMM_USER));
  assert(vmm_map_range(&a.address_space,UINT64_C(0xffff800000000000),kernel_page,
                       VMM_PAGE_SIZE,VMM_READ|VMM_USER,0)==VMM_INVALID_ARGUMENT);
  assert(vmm_map_range(&a.address_space,UINT64_C(0x00400000),kernel_page,
                       VMM_PAGE_SIZE,VMM_READ|VMM_USER,0)==VMM_INVALID_ARGUMENT);
  *(volatile uint32_t *)access_frame(&m,a.data_frame)=0x11;
  *(volatile uint32_t *)access_frame(&m,b.data_frame)=0x22;
  assert(*(volatile uint32_t *)access_frame(&m,a.data_frame)==0x11);
  assert(*(volatile uint32_t *)access_frame(&m,b.data_frame)==0x22);
  process_note_fault(&a,PROCESS_USER_INVALID,2);
  uint64_t fault=0,code=0;
  assert(process_wait(&a,&fault,&code)==PROCESS_FAULTED && fault==PROCESS_USER_INVALID && code==2);
  assert(process_request_kill(&b)==PROCESS_OK);
  assert(b.kill_requested && process_wait(&b,0,0)==PROCESS_READY);
  b.state=PROCESS_KILLED;
  assert(process_destroy(&a)==PROCESS_OK && process_destroy(&b)==PROCESS_OK);
  assert(vmm_destroy(&kernel)==VMM_OK);
  return 0;
}
