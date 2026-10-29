/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/boot/contract.h"
#if defined(LAMIX_M09_PMM)
#include "pmm.h"
#endif
#include <stddef.h>
#include <stdint.h>

#define IDT_ENTRIES 256
#define COM1 0x3f8
#define PIC1 0x20
#define PIC2 0xa0
#define PIT 0x40
#define KCODE 0x08
#define KDATA 0x10
#define KTSS 0x1f0
#define PAGE_SIZE UINT64_C(4096)
#define PTE_PRESENT UINT64_C(1)
#define PTE_WRITE UINT64_C(2)
#define PTE_HUGE UINT64_C(0x80)
#define PTE_ADDRESS UINT64_C(0x000ffffffffff000)
#define PTE_NX UINT64_C(0x8000000000000000)

typedef struct __attribute__((packed)) { uint16_t limit; uint64_t base; } desc_ptr;
typedef struct __attribute__((packed)) { uint16_t off0, selector; uint8_t ist, attr; uint16_t off1; uint32_t off2, zero; } idt_gate;
typedef struct __attribute__((packed)) { uint32_t r0; uint64_t rsp0,rsp1,rsp2; uint64_t r1; uint64_t ist1,ist2,ist3,ist4,ist5,ist6,ist7; uint64_t r2; uint16_t r3,iomap; } tss64;
typedef struct interrupt_frame {
  uint64_t r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rbp,rbx,rdx,rcx,rax;
  uint64_t vector,error,rip,cs,rflags,rsp,ss;
} interrupt_frame;
_Static_assert(offsetof(interrupt_frame, vector)==120,"frame.vector");
_Static_assert(offsetof(interrupt_frame, rip)==136,"frame.rip");
_Static_assert(sizeof(idt_gate)==16,"IDT gate");
_Static_assert(offsetof(idt_gate,selector)==2 && offsetof(idt_gate,ist)==4 && offsetof(idt_gate,attr)==5 && offsetof(idt_gate,off2)==8,"IDT gate fields");
_Static_assert(sizeof(tss64)==104,"TSS");
_Static_assert(offsetof(tss64,ist1)==36 && offsetof(tss64,iomap)==102,"TSS fields");

extern uintptr_t interrupt_vectors[IDT_ENTRIES];
static idt_gate idt[IDT_ENTRIES] __attribute__((aligned(16)));
static uint64_t gdt[64] __attribute__((aligned(16)));
static tss64 tss;
static uint8_t df_stack[16384] __attribute__((aligned(16)));
static volatile uint32_t ticks;
static uint8_t serial_ready;
static uint64_t acpi_scratch_base, acpi_scratch_pages;
#if defined(LAMIX_M09_PMM)
static pmm physical_memory;
static uint8_t pmm_states[PMM_STATE_BYTES];
#endif
static __attribute__((noreturn)) void fail(void);

static inline uint8_t in8(uint16_t p){uint8_t x;__asm__ volatile("inb %1,%0":"=a"(x):"Nd"(p));return x;}
static inline void out8(uint16_t p,uint8_t x){__asm__ volatile("outb %0,%1"::"a"(x),"Nd"(p));}
static void serial_char(char c){if(!serial_ready)return;for(uint32_t n=100000;n && !(in8(COM1+5)&0x20);--n){}out8(COM1,(uint8_t)c);}
static void serial(const char*s){while(*s)serial_char(*s++);}
static void hex(uint64_t n){static const char d[]="0123456789abcdef";for(int i=60;i>=0;i-=4)serial_char(d[(n>>i)&15]);}
static void serial_init(void){out8(COM1+1,0);out8(COM1+3,0x80);out8(COM1,1);out8(COM1+1,0);out8(COM1+3,3);out8(COM1+2,0xc7);out8(COM1+4,0x0b);serial_ready=1;}
__attribute__((noreturn)) static void halt(void){__asm__ volatile("cli");for(;;)__asm__ volatile("hlt");}
__attribute__((noreturn)) static void panic_frame(const interrupt_frame*f){
  serial("M08:PANIC\nvector=");hex(f->vector);serial(" error=");hex(f->error);serial(" rip=");hex(f->rip);serial(" cs=");hex(f->cs);serial(" rflags=");hex(f->rflags);serial(" rsp=");hex(f->rsp);
  serial(" rax=");hex(f->rax);serial(" rbx=");hex(f->rbx);serial(" rcx=");hex(f->rcx);serial(" rdx=");hex(f->rdx);serial(" rsi=");hex(f->rsi);serial(" rdi=");hex(f->rdi);serial(" rbp=");hex(f->rbp);serial(" r8=");hex(f->r8);serial(" r9=");hex(f->r9);serial(" r10=");hex(f->r10);serial(" r11=");hex(f->r11);serial(" r12=");hex(f->r12);serial(" r13=");hex(f->r13);serial(" r14=");hex(f->r14);serial(" r15=");hex(f->r15);
  if(f->vector==14){uint64_t cr2;__asm__ volatile("mov %%cr2,%0":"=r"(cr2));serial(" cr2=");hex(cr2);}serial("\n");halt();
}
static uint16_t gate_selector=KCODE;
static void gate(unsigned v,uintptr_t a,uint8_t ist){idt[v]=(idt_gate){(uint16_t)a,gate_selector,(uint8_t)(ist&7),0x8e,(uint16_t)(a>>16),(uint32_t)(a>>32),0};}
static void idt_init(uint16_t selector,int final){
  gate_selector=selector;
  for(unsigned i=0;i<IDT_ENTRIES;i++)gate(i,interrupt_vectors[i],final && i==8?1:0);
  desc_ptr p={(uint16_t)(sizeof(idt)-1),(uint64_t)idt};__asm__ volatile("lidt %0"::"m"(p):"memory");
}
static void gdt_init(uint16_t prior_cs){
  gdt[1]=UINT64_C(0x00af9a000000ffff);gdt[2]=UINT64_C(0x00cf92000000ffff);
  unsigned old_index=(prior_cs&0xfff8)>>3;
  if((prior_cs&4) || (prior_cs&3) || old_index>=62) fail();
  gdt[old_index]=UINT64_C(0x00af9a000000ffff);
  tss.iomap=sizeof(tss);tss.ist1=(uint64_t)(df_stack+sizeof(df_stack));
  uint64_t b=(uint64_t)&tss;gdt[62]=((uint64_t)(sizeof(tss)-1))|((b&0xffffff)<<16)|UINT64_C(0x890000000000)|((b&0xff000000)<<32);gdt[63]=b>>32;
  desc_ptr p={(uint16_t)(sizeof(gdt)-1),(uint64_t)gdt};__asm__ volatile("lgdt %0"::"m"(p):"memory");
  __asm__ volatile("pushq $0x08; lea 1f(%%rip),%%rax; push %%rax; lretq; 1: mov $0x10,%%ax; mov %%ax,%%ds; mov %%ax,%%es; mov %%ax,%%ss; mov %0,%%ax; ltr %%ax" ::"i"(KTSS):"rax","memory");
}
static void pic_pit_init(void){out8(0x21,0xff);out8(0xa1,0xff);out8(PIC1,0x11);out8(PIC2,0x11);out8(PIC1+1,0x20);out8(PIC2+1,0x28);out8(PIC1+1,4);out8(PIC2+1,2);out8(PIC1+1,1);out8(PIC2+1,1);out8(0x21,0xfe);out8(0xa1,0xff);uint16_t divisor=(uint16_t)(1193182/100);out8(PIT+3,0x36);out8(PIT,(uint8_t)divisor);out8(PIT,(uint8_t)(divisor>>8));}
void interrupt_dispatch(interrupt_frame*f){if(f->vector==32){ticks++;serial("M08:TICK\n");out8(PIC1,0x20);return;}panic_frame(f);}
static const uint8_t *record(const lbp_view*v,uint32_t type){for(uint32_t i=0;i<v->count;i++){uint64_t at;if(lbp_record_at(v,i,&at)!=LBP_OK) return 0;const uint8_t*p=v->bytes+at;if(*(const uint32_t*)p==type)return p;}return 0;}
#if defined(LAMIX_M09_PMM)
static int frame_usable(const lbp_view *view, uint64_t physical) {
  const uint8_t *raw=record(view,LBP_MEMORY_MAP);
  if(!raw||((const lbp_record*)raw)->size<32)return 0;
  uint32_t stride=*(const uint32_t*)(raw+20);
  uint64_t count=*(const uint64_t*)(raw+24),size=((const lbp_record*)raw)->size;
  if(stride<32||count>(size-32)/stride)return 0;
  for(uint64_t i=0;i<count;i++){
    const uint8_t *entry=raw+32+i*stride;
    uint64_t base=*(const uint64_t*)entry,length=*(const uint64_t*)(entry+8);
    if(*(const uint32_t*)(entry+16)==LBP_USABLE&&physical>=base&&
       physical-base<=length&&PMM_PAGE_SIZE<=length-(physical-base))return 1;
  }
  return 0;
}
static int frame_reserved(const lbp_view *view, uint64_t physical) {
  const uint8_t *raw=record(view,LBP_RESERVATIONS);
  if(!raw||((const lbp_record*)raw)->size<32)return 1;
  uint32_t stride=*(const uint32_t*)(raw+20);
  uint64_t count=*(const uint64_t*)(raw+24),size=((const lbp_record*)raw)->size;
  if(stride<32||count>(size-32)/stride)return 1;
  for(uint64_t i=0;i<count;i++){
    const uint8_t *entry=raw+32+i*stride;
    uint64_t base=*(const uint64_t*)entry,length=*(const uint64_t*)(entry+8);
    if(physical>=base&&physical-base<length)return 1;
  }
  return 0;
}
static int pmm_self_test(const lbp_view *view,const lbp_entry *entry){
  uint64_t frames[8],initial=0;
  if(pmm_init(&physical_memory,view,pmm_states,sizeof(pmm_states))!=PMM_OK)return 0;
  initial=physical_memory.free_frames;
  if(initial<8)return 0;
  for(unsigned i=0;i<8;i++){
    if(pmm_alloc_frame(&physical_memory,&frames[i])!=PMM_OK||
       frames[i]>=PMM_PHYSICAL_LIMIT||frames[i]%PMM_PAGE_SIZE||
       !frame_usable(view,frames[i])||frame_reserved(view,frames[i]))return 0;
    for(unsigned j=0;j<i;j++)if(frames[j]==frames[i])return 0;
  }
  if(pmm_free_frame(&physical_memory,frames[0])!=PMM_OK||
     pmm_free_frame(&physical_memory,frames[0])!=PMM_NOT_ALLOCATED||
     pmm_free_frame(&physical_memory,frames[1]+1)!=PMM_INVALID_ARGUMENT||
     pmm_free_frame(&physical_memory,entry->cr3)!=PMM_NOT_ALLOCATED)return 0;
  for(unsigned i=1;i<8;i++)
    if(pmm_free_frame(&physical_memory,frames[i])!=PMM_OK)return 0;
  if(physical_memory.free_frames!=initial||physical_memory.allocated_frames!=0)return 0;
  serial("M09:PMM frames=8 free=");hex(initial);serial("\n");
  return 1;
}
#endif
static const uint8_t *raw_record(const lbp_header*h,uint32_t wanted){
  uint64_t total=h->total_size,at=h->records_offset;uint32_t count=h->record_count;
  if(total<sizeof(*h)||total>LBP_MAX_SIZE||count>LBP_MAX_RECORDS||at<sizeof(*h)||at>total)return 0;
  for(uint32_t i=0;i<count;i++){
    if(at>total-sizeof(lbp_record))return 0;
    const lbp_record*r=(const lbp_record*)((const uint8_t*)h+at);
    if(r->size<sizeof(*r)||(r->size&7)||r->size>total-at)return 0;
    if(r->type==wanted)return (const uint8_t*)r;
    at+=r->size;
  }
  return 0;
}
static int table_reservation(const lbp_header*h,uint64_t*base,uint64_t*length){
  const uint8_t*r=raw_record(h,LBP_RESERVATIONS);if(!r||((const lbp_record*)r)->size<32)return 0;
  uint32_t stride=*(const uint32_t*)(r+20);uint64_t count=*(const uint64_t*)(r+24),size=((const lbp_record*)r)->size;
  if(stride<32||(stride&7)||count>(size-32)/stride)return 0;
  for(uint64_t i=0;i<count;i++){const uint8_t*q=r+32+i*stride;
    if(*(const uint32_t*)(q+16)==LBP_PURPOSE_TABLES&&*(const uint32_t*)(q+20)==LBP_RELEASE_REPLACED){
      *base=*(const uint64_t*)q;*length=*(const uint64_t*)(q+8);
      if(!*length||*length>UINT64_C(0x100000000)||(*base&(PAGE_SIZE-1))||(*length&(PAGE_SIZE-1))||*base>UINT64_C(0x100000000)-*length)return 0;
      return 1;
    }
  }
  return 0;
}
static uint64_t allocate_table_page(uint64_t base,uint64_t length){
  for(uint64_t off=0;off<length;off+=PAGE_SIZE){volatile uint64_t*p=(volatile uint64_t*)(uintptr_t)(base+off);unsigned i=0;
    for(;i<512&&p[i]==0;i++){}
    if(i==512){for(i=0;i<512;i++)p[i]=0;return base+off;}
  }
  return 0;
}
static volatile uint64_t *page_leaf(uint64_t root,uint64_t va,uint64_t table_base,uint64_t table_size,int create){
  volatile uint64_t*t=(volatile uint64_t*)(uintptr_t)(root&PTE_ADDRESS);
  for(unsigned shift=39;shift>12;shift-=9){volatile uint64_t*slot=&t[(va>>shift)&511];
    if(!(*slot&PTE_PRESENT)){
      if(!create)return 0;
      uint64_t next=allocate_table_page(table_base,table_size);
      if(!next)return 0;
      *slot=next|PTE_PRESENT|PTE_WRITE;
    }else if(*slot&PTE_HUGE)return 0;
    t=(volatile uint64_t*)(uintptr_t)(*slot&PTE_ADDRESS);
  }
  return &t[(va>>12)&511];
}
static void invalidate(uint64_t va){__asm__ volatile("invlpg (%0)"::"r"((uintptr_t)va):"memory");}
static int map_acpi_span(const lbp_header*h,const lbp_acpi*a,lbp_span*span){
  uint64_t physical=a->physical,length=a->length,end=physical+length,table_base,table_size;
  if((length!=20&&length!=36)||end<physical||end>UINT64_C(0x0010000000000000)||!table_reservation(h,&table_base,&table_size))return 0;
  uint64_t first=physical&~(PAGE_SIZE-1),last=(end+PAGE_SIZE-1)&~(PAGE_SIZE-1);
  if(last<end||last<first)return 0;
  acpi_scratch_pages=(last-first)/PAGE_SIZE;
  uint64_t root;__asm__ volatile("mov %%cr3,%0":"=r"(root));
  int mapped=0;
  for(unsigned candidate=0;candidate<16&&!mapped;candidate++){
    uint64_t base=UINT64_C(0xfffffe0000000000)+(uint64_t)candidate*UINT64_C(0x200000);int free=1;
    for(uint64_t i=0;i<acpi_scratch_pages;i++){volatile uint64_t*p=page_leaf(root,base+i*PAGE_SIZE,table_base,table_size,0);if(p&&(*p&PTE_PRESENT)){free=0;break;}}
    if(!free)continue;
    for(uint64_t i=0;i<acpi_scratch_pages;i++){
      volatile uint64_t*p=page_leaf(root,base+i*PAGE_SIZE,table_base,table_size,1);
      if(!p||(*p&PTE_PRESENT)){free=0;break;}
      *p=(first+i*PAGE_SIZE)|PTE_PRESENT;
      if(LBP_FEATURE_NX&((const lbp_header*)h)->provided_features)*p|=PTE_NX;
      invalidate(base+i*PAGE_SIZE);
    }
    if(free){acpi_scratch_base=base;span->physical=physical;span->bytes=(const uint8_t*)(uintptr_t)(base+(physical-first));span->size=length;mapped=1;}
    else for(uint64_t i=0;i<acpi_scratch_pages;i++){volatile uint64_t*p=page_leaf(root,base+i*PAGE_SIZE,table_base,table_size,0);if(p){*p=0;invalidate(base+i*PAGE_SIZE);}}
  }
  return mapped;
}
static void unmap_acpi_span(void){
  uint64_t root;__asm__ volatile("mov %%cr3,%0":"=r"(root));
  for(uint64_t i=0;i<acpi_scratch_pages;i++){volatile uint64_t*p=page_leaf(root,acpi_scratch_base+i*PAGE_SIZE,0,0,0);if(p){*p=0;invalidate(acpi_scratch_base+i*PAGE_SIZE);}}
  acpi_scratch_base=acpi_scratch_pages=0;
}
static __attribute__((noreturn)) void fail(void){serial("M08:FAIL\n");halt();}
__attribute__((noreturn)) void kernel_main(uint64_t self,uint64_t magic){
  serial_init();serial("M08:ENTRY\n");if(magic!=LBP_ENTRY_MAGIC||!self)fail();const lbp_header*h=(const lbp_header*)(uintptr_t)self;lbp_view view;
  if(h->self_phys!=self||__builtin_memcmp(h->magic,LBP_MAGIC,8))fail();
  const uint8_t*raw_acpi=raw_record(h,LBP_ACPI);lbp_span span,*spans=0;uint64_t span_count=0;
  if(raw_acpi){if(((const lbp_record*)raw_acpi)->size<sizeof(lbp_acpi)||!map_acpi_span(h,(const lbp_acpi*)raw_acpi,&span))fail();spans=&span;span_count=1;}
  lbp_status validation=lbp_validate(h,h->total_size,spans,span_count,&view);
  if(raw_acpi)unmap_acpi_span();
  if(validation!=LBP_OK)fail();
  const lbp_entry*e=(const lbp_entry*)record(&view,LBP_ENTRY);
  if(!e)fail();
#if defined(LAMIX_M09_PMM)
  if(!pmm_self_test(&view,e)){serial("M09:FAIL\n");fail();}
#endif
  uint16_t prior_cs;__asm__ volatile("mov %%cs,%0":"=r"(prior_cs));
  idt_init(prior_cs,0);
  gdt_init(prior_cs);
  idt_init(KCODE,1);
  serial("M08:LBP\n");
  serial("M08:GUARD=");hex(e->guard);serial("\n");
#if defined(LAMIX_FAULT_UD)
  __asm__ volatile("ud2");
#elif defined(LAMIX_FAULT_PF)
  *(volatile uint8_t*)(uintptr_t)e->guard=0x5a;
#endif
  pic_pit_init();__asm__ volatile("sti");while(ticks<3)__asm__ volatile("pause");__asm__ volatile("cli");out8(0x21,0xff);out8(0xa1,0xff);serial("M08:PASS\n");
#if defined(LAMIX_M09_PMM)
  serial("M09:PASS\n");
#endif
  halt();
}
