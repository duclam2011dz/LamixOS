/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/boot/contract.h"
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
static __attribute__((noreturn)) void fail(void){serial("M08:FAIL\n");halt();}
__attribute__((noreturn)) void kernel_main(uint64_t self,uint64_t magic){
  serial_init();serial("M08:ENTRY\n");if(magic!=LBP_ENTRY_MAGIC||!self)fail();const lbp_header*h=(const lbp_header*)(uintptr_t)self;lbp_view view;
  if(lbp_validate(h,h->total_size,0,0,&view)!=LBP_OK||h->self_phys!=self)fail();
  const lbp_entry*e=(const lbp_entry*)record(&view,LBP_ENTRY);
  if(!e)fail();
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
  pic_pit_init();__asm__ volatile("sti");while(ticks<3)__asm__ volatile("pause");__asm__ volatile("cli");out8(0x21,0xff);out8(0xa1,0xff);serial("M08:PASS\n");halt();
}
