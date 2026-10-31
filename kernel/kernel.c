/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/boot/contract.h"
#if defined(LAMIX_M09_PMM)
#include "pmm.h"
#endif
#if defined(LAMIX_M10_VMM)
#include "vmm.h"
#endif
#if defined(LAMIX_M11_HEAP)
#include "heap.h"
#endif
#if defined(LAMIX_M13_DEVICES)
#include "acpi.h"
#include "device.h"
#include "pci.h"
#endif
#if defined(LAMIX_M14_APIC)
#include "apic.h"
#endif
#if defined(LAMIX_M15_INPUT)
#include "input.h"
#include "ps2.h"
#endif
#if defined(LAMIX_M16_GRAPHICS)
#include "graphics.h"
#endif
#if defined(LAMIX_M17_VIRTIO_GPU)
#include "virtio_gpu.h"
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
#if defined(LAMIX_M14_APIC)
static volatile uint32_t *m14_lapic;
static volatile uint32_t *m14_ioapics[ACPI_MADT_MAX_IOAPICS];
static uint32_t m14_ioapic_max[ACPI_MADT_MAX_IOAPICS];
static volatile uint32_t m14_pit_irqs, m14_timer_irqs;
static volatile uint32_t m14_pit_eois, m14_timer_eois;
static volatile uint32_t m14_calibration_start, m14_calibration_end;
static volatile uint8_t m14_active;
#if defined(LAMIX_M15_INPUT)
static acpi_madt_info m15_madt;
static uint8_t m15_bsp_id;
static input_state m15_input;
static ps2_controller m15_ps2;
static volatile uint32_t m15_keyboard_irqs, m15_mouse_irqs;
#endif
#define M14_PIT_CALIBRATION_INTERVALS 10u
#define M14_PIT_CALIBRATION_IRQS (M14_PIT_CALIBRATION_INTERVALS + 1u)
#endif
static uint64_t acpi_scratch_base, acpi_scratch_pages;
#if defined(LAMIX_M09_PMM)
static pmm physical_memory;
static uint8_t pmm_states[PMM_STATE_BYTES];
#endif
#if defined(LAMIX_M12_BUDDY)
static pmm_buddy experimental_buddy;
static uint8_t buddy_node_storage[PMM_BUDDY_STATE_BYTES];
static uint32_t buddy_boundary_storage[PMM_BUDDY_BOUNDARY_CAPACITY];
#endif
#if defined(LAMIX_M10_VMM)
static vmm_space kernel_space;
static uint64_t vmm_scratch_base, vmm_scratch_control;
static uint64_t vmm_scratch_table_bytes, vmm_scratch_control_page;
static uint64_t vmm_test_physical;
static uint8_t vmm_scratch_active, vmm_nx_enabled;
static const uint64_t VMM_SCRATCH_BASE = UINT64_C(0xfffffd0000000000);
static const uint64_t VMM_SCRATCH_CONTROL = UINT64_C(0xfffffd0000001000);
static const uint64_t VMM_TEST_BASE = UINT64_C(0xfffffc0000000000);
#endif
#if defined(LAMIX_M11_HEAP)
static kheap kernel_heap;
extern int m11_cpp_heap_probe(kheap *heap);
#endif
#if defined(LAMIX_M13_DEVICES)
#define M13_TEMP_BASE UINT64_C(0xfffffa0000000000)
#define M13_ECAM_BASE UINT64_C(0xfffffb0000000000)
#define M13_BAR_BASE UINT64_C(0xfffffc0000000000)
#define M13_MAX_DEVICES DEVICE_REGISTRY_CAPACITY
static uint8_t m13_rsdp[ACPI_RSDP_V2_SIZE];
static size_t m13_rsdp_size;
static uint8_t m13_acpi_scratch[ACPI_MAX_TABLE_SIZE];
static pci_mcfg_region m13_mcfg_regions[PCI_MAX_MCFG_REGIONS];
static size_t m13_mcfg_count;
static uint64_t m13_temp_mapped = UINT64_MAX;
static uint64_t m13_ecam_mapped = UINT64_MAX;
static pci_ecam_ops m13_ecam_ops;
static pci_hybrid_ops m13_hybrid_context;
static pci_config_ops m13_config;
static pci_function m13_functions[M13_MAX_DEVICES];
static device_record m13_device_storage[M13_MAX_DEVICES];
static device_registry m13_registry;
#endif
#if defined(LAMIX_M17_VIRTIO_GPU)
#define M17_GPU_VA UINT64_C(0xfffff60000000000)
#define M17_GPU_BAR_STRIDE UINT64_C(0x40000000)
#define M17_GPU_RESOURCE_LIMIT (UINT64_C(16) * 1024 * 1024)
typedef struct m17_gpu_pci_context {
  volatile uint8_t *common;
  volatile uint8_t *notify;
  uint32_t notify_multiplier;
  uint32_t notify_length;
  uint8_t valid;
} m17_gpu_pci_context;
static m17_gpu_pci_context m17_pci_context;
static virtio_gpu m17_gpu;
static virtio_gpu_memory m17_memory;
static uint8_t m17_descriptors[4096] __attribute__((aligned(4096)));
static uint8_t m17_available[4096] __attribute__((aligned(4096)));
static uint8_t m17_used[4096] __attribute__((aligned(4096)));
static uint8_t m17_commands[sizeof(uint32_t) * 8 +
    VIRTIO_GPU_MAX_BACKING_PAGES * 16] __attribute__((aligned(4096)));
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
  if(f->vector==14){uint64_t cr2;__asm__ volatile("mov %%cr2,%0":"=r"(cr2));serial(" cr2=");hex(cr2);}serial("\n");
#if defined(LAMIX_M10_VMM)
  serial("M10:PANIC\n");
#endif
  halt();
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
void interrupt_dispatch(interrupt_frame*f){if(f->vector==32){
#if defined(LAMIX_M14_APIC)
  if (m14_active) panic_frame(f);
#endif
  ticks++;serial("M08:TICK\n");out8(PIC1,0x20);return;}
#if defined(LAMIX_M14_APIC)
  if (f->vector == 0x30 || f->vector == 0x31) {
    if (!m14_lapic) panic_frame(f);
    if (f->vector == 0x30) {
      uint32_t irq = ++m14_pit_irqs;
      if (irq == 1) m14_calibration_start = m14_lapic[0x390 / 4];
      else if (irq == M14_PIT_CALIBRATION_IRQS)
        m14_calibration_end = m14_lapic[0x390 / 4];
      ++m14_pit_eois;
    } else { ++m14_timer_irqs; ++m14_timer_eois; }
    __asm__ volatile("mfence" ::: "memory");
    m14_lapic[0xb0 / 4] = 0;
    return;
  }
#if defined(LAMIX_M15_INPUT)
  if (f->vector == 0x32 || f->vector == 0x33) {
    uint8_t byte;
    int auxiliary;
    if (f->vector == 0x32) ++m15_keyboard_irqs;
    else ++m15_mouse_irqs;
    if (ps2_read_irq_byte(&m15_ps2, &byte, &auxiliary)) {
      if (auxiliary) input_mouse_byte(&m15_input, byte);
      else input_keyboard_scancode(&m15_input, byte);
    }
    __asm__ volatile("mfence" ::: "memory");
    m14_lapic[0xb0 / 4] = 0;
    return;
  }
#endif
  if (f->vector == 0xff) return;
#endif
  panic_frame(f);}
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

#if defined(LAMIX_M12_BUDDY)
static int pmm_buddy_self_test(const lbp_view *view) {
  pmm_buddy_stats stats;
  uint64_t frame = 0, block = 0;
  if (pmm_buddy_init(&experimental_buddy, view, buddy_node_storage,
                     sizeof(buddy_node_storage), buddy_boundary_storage,
                     PMM_BUDDY_BOUNDARY_CAPACITY) != PMM_OK ||
      pmm_buddy_get_stats(&experimental_buddy, &stats) != PMM_OK ||
      stats.free_frames < 3 ||
      pmm_buddy_alloc_frame(&experimental_buddy, &frame) != PMM_OK ||
      frame >= PMM_PHYSICAL_LIMIT || !frame_usable(view, frame) ||
      frame_reserved(view, frame) ||
      pmm_buddy_free_frame(&experimental_buddy, frame) != PMM_OK ||
      pmm_buddy_free_frame(&experimental_buddy, frame) != PMM_NOT_ALLOCATED)
    return 0;
  uint32_t initial_free = stats.free_frames;
  uint32_t order = PMM_BUDDY_MAX_ORDER;
  while (!experimental_buddy.free_blocks[order] && order)
    --order;
  if (pmm_buddy_alloc_order(&experimental_buddy, order, &block) != PMM_OK)
    return 0;
  uint64_t block_bytes = PMM_PAGE_SIZE << order;
  int contained = 0;
  const uint8_t *raw = record(view, LBP_MEMORY_MAP);
  if (!raw || ((const lbp_record *)raw)->size < 32)
    return 0;
  uint32_t stride = *(const uint32_t *)(raw + 20);
  uint64_t count = *(const uint64_t *)(raw + 24);
  for (uint64_t i = 0; i < count; ++i) {
    const uint8_t *entry = raw + 32 + i * stride;
    uint64_t base = *(const uint64_t *)entry;
    uint64_t length = *(const uint64_t *)(entry + 8);
    if (*(const uint32_t *)(entry + 16) == LBP_USABLE && block >= base &&
        block - base <= length && block_bytes <= length - (block - base)) {
      contained = 1;
      break;
    }
  }
  const uint8_t *reserved = record(view, LBP_RESERVATIONS);
  if (!reserved || ((const lbp_record *)reserved)->size < 32)
    return 0;
  uint32_t reserved_stride = *(const uint32_t *)(reserved + 20);
  uint64_t reserved_count = *(const uint64_t *)(reserved + 24);
  for (uint64_t i = 0; i < reserved_count; ++i) {
    const uint8_t *entry = reserved + 32 + i * reserved_stride;
    uint64_t base = *(const uint64_t *)entry;
    uint64_t length = *(const uint64_t *)(entry + 8);
    if (block < base + length && base < block + block_bytes)
      return 0;
  }
  if (!contained || (block & (block_bytes - 1u)) ||
      pmm_buddy_free_order(&experimental_buddy, block, order) != PMM_OK ||
      pmm_buddy_get_stats(&experimental_buddy, &stats) != PMM_OK ||
      stats.allocated_frames != 0 || stats.free_frames != initial_free)
    return 0;
  serial("M12:BUDDY order=");
  hex(order);
  serial(" free=");
  hex(stats.free_frames);
  serial(" metadata=");
  hex(PMM_BUDDY_STATE_BYTES +
      sizeof(buddy_boundary_storage));
  serial("\n");
  return 1;
}
#endif

#if defined(LAMIX_M10_VMM)
#if defined(LAMIX_M12_BUDDY) && !defined(LAMIX_M13_DEVICES)
static int kernel_alloc_frame(uint64_t *physical) {
  return pmm_buddy_alloc_frame(&experimental_buddy, physical) == PMM_OK;
}

static int kernel_free_frame(uint64_t physical) {
  return pmm_buddy_free_frame(&experimental_buddy, physical) == PMM_OK;
}
#else
static int kernel_alloc_frame(uint64_t *physical) {
  return pmm_alloc_frame(&physical_memory, physical) == PMM_OK;
}

static int kernel_free_frame(uint64_t physical) {
  return pmm_free_frame(&physical_memory, physical) == PMM_OK;
}
#endif
#endif
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
static int table_referenced(uint64_t table, uint64_t wanted, unsigned level) {
  if (table == wanted)
    return 1;
  if (level <= 1)
    return 0;
  volatile uint64_t *entries = (volatile uint64_t *)(uintptr_t)table;
  for (unsigned i = 0; i < 512; ++i) {
    uint64_t entry = entries[i];
    if (!(entry & PTE_PRESENT) || (entry & PTE_HUGE))
      continue;
    uint64_t child = entry & PTE_ADDRESS;
    if (child == wanted || table_referenced(child, wanted, level - 1))
      return 1;
  }
  return 0;
}
static uint64_t allocate_table_page(uint64_t root,uint64_t base,uint64_t length){
  for(uint64_t off=0;off<length;off+=PAGE_SIZE){uint64_t candidate=base+off;if(table_referenced(root,candidate,4))continue;volatile uint64_t*p=(volatile uint64_t*)(uintptr_t)candidate;unsigned i=0;
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
      uint64_t next=allocate_table_page(root,table_base,table_size);
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
#if defined(LAMIX_M13_DEVICES) && defined(LAMIX_M10_VMM)
static volatile uint8_t *m13_map_page(uint64_t virtual_address,
                                      uint64_t physical, uint64_t *cached) {
  uint64_t page = physical & ~(PAGE_SIZE - 1);
  if (*cached != page) {
    if (*cached != UINT64_MAX &&
        vmm_unmap_range(&kernel_space, virtual_address, PAGE_SIZE) != VMM_OK)
      return 0;
    if (vmm_map_range(&kernel_space, virtual_address, page, PAGE_SIZE,
                      VMM_READ | VMM_WRITE, VMM_MAP_UNCACHED) != VMM_OK) {
      *cached = UINT64_MAX;
      return 0;
    }
    *cached = page;
  }
  return (volatile uint8_t *)(uintptr_t)(virtual_address + (physical - page));
}

static int m13_read_physical(void *context, uint64_t physical,
                             void *destination, size_t length) {
  (void)context;
  uint8_t *out = (uint8_t *)destination;
  if (!destination || physical >= VMM_PHYSICAL_LIMIT ||
      (uint64_t)length > VMM_PHYSICAL_LIMIT - physical)
    return 0;
  while (length) {
    size_t chunk = PAGE_SIZE - (size_t)(physical & (PAGE_SIZE - 1));
    if (chunk > length)
      chunk = length;
    volatile uint8_t *mapped = m13_map_page(M13_TEMP_BASE, physical,
                                             &m13_temp_mapped);
    if (!mapped)
      return 0;
    for (size_t i = 0; i < chunk; ++i)
      out[i] = mapped[i];
    physical += chunk;
    out += chunk;
    length -= chunk;
  }
  return 1;
}

static volatile uint32_t *m13_map_ecam_dword(void *context,
                                              uint64_t physical) {
  (void)context;
  volatile uint8_t *mapped = m13_map_page(M13_ECAM_BASE, physical,
                                           &m13_ecam_mapped);
  return mapped ? (volatile uint32_t *)(uintptr_t)mapped : 0;
}

static int m13_map_bar(void *context, uint64_t virtual_address,
                       uint64_t physical_address, uint64_t length) {
  (void)context;
  return vmm_map_range(&kernel_space, virtual_address, physical_address,
                       length, VMM_READ | VMM_WRITE,
                       VMM_MAP_UNCACHED) == VMM_OK;
}

static int m13_unmap_bar(void *context, uint64_t virtual_address,
                         uint64_t length) {
  (void)context;
  return vmm_unmap_range(&kernel_space, virtual_address, length) == VMM_OK;
}

static void m13_release_mappings(void) {
  if (m13_temp_mapped != UINT64_MAX) {
    (void)vmm_unmap_range(&kernel_space, M13_TEMP_BASE, PAGE_SIZE);
    m13_temp_mapped = UINT64_MAX;
  }
  if (m13_ecam_mapped != UINT64_MAX) {
    (void)vmm_unmap_range(&kernel_space, M13_ECAM_BASE, PAGE_SIZE);
    m13_ecam_mapped = UINT64_MAX;
  }
}

static int m13_platform_init(void) {
  if (m13_rsdp_size) {
    acpi_platform platform = {0};
    acpi_status status = acpi_discover(m13_rsdp, m13_rsdp_size,
                                       m13_read_physical, 0,
                                       m13_acpi_scratch,
                                       sizeof(m13_acpi_scratch), &platform);
    if (status != ACPI_OK) {
      serial("M13:ACPI:INVALID status="); hex(status); serial("\n");
    } else {
      uint32_t madt_entries = 0;
      if (platform.madt_physical) {
        if (!m13_read_physical(0, platform.madt_physical, m13_acpi_scratch,
                               platform.madt_length))
          return 0;
        size_t cursor = 44;
        acpi_madt_entry entry;
        for (;;) {
          status = acpi_madt_entry_at(m13_acpi_scratch,
                                      platform.madt_length, &cursor, &entry);
          if (status == ACPI_UNSUPPORTED)
            break;
          if (status != ACPI_OK)
            return 0;
          ++madt_entries;
        }
      }
      if (platform.mcfg_physical) {
        if (!m13_read_physical(0, platform.mcfg_physical, m13_acpi_scratch,
                               platform.mcfg_length))
          return 0;
        if (pci_parse_mcfg(m13_acpi_scratch, platform.mcfg_length,
                           m13_mcfg_regions, PCI_MAX_MCFG_REGIONS,
                           &m13_mcfg_count) != PCI_OK)
          m13_mcfg_count = 0;
      }
      serial("M13:ACPI:PASS madt="); hex(madt_entries);
      serial(" mcfg="); hex(m13_mcfg_count); serial("\n");
    }
  } else {
    serial("M13:ACPI:ABSENT\n");
  }

  pci_cf8_ops(&m13_hybrid_context.legacy);
  m13_ecam_ops = (pci_ecam_ops){0, m13_mcfg_regions, m13_mcfg_count,
                                m13_map_ecam_dword};
  m13_hybrid_context.ecam = m13_ecam_ops;
  pci_hybrid_config_ops(&m13_hybrid_context, &m13_config);
  size_t function_count = 0;
  if (pci_enumerate_mcfg(&m13_config, m13_mcfg_regions, m13_ecam_ops.region_count,
                         m13_functions, M13_MAX_DEVICES,
                         &function_count) != PCI_OK || !function_count)
    return 0;
  if (device_registry_init(&m13_registry, m13_device_storage,
                           M13_MAX_DEVICES) != DEVICE_OK)
    return 0;
  for (size_t i = 0; i < function_count; ++i) {
    if (pci_probe_bars(&m13_config, &m13_functions[i]) != PCI_OK)
      return 0;
    size_t index = 0;
    if (device_register_pci(&m13_registry, &m13_functions[i], &index) !=
        DEVICE_OK)
      return 0;
  }
  pci_mmio_ops mmio = {0, m13_map_bar, m13_unmap_bar};
  int mapped_bar = 0;
  for (size_t i = 0; i < function_count && !mapped_bar; ++i) {
    for (unsigned bar = 0; bar < PCI_BAR_COUNT; ++bar) {
      pci_bar *resource = &m13_functions[i].bars[bar];
      if (!resource->valid || resource->is_io || !resource->base ||
          resource->size < PAGE_SIZE)
        continue;
      if (pci_map_bar(&mmio, &m13_functions[i], bar, 0, M13_BAR_BASE,
                      PAGE_SIZE) != PCI_OK)
        return 0;
      uint64_t mapped_physical = 0, flags = 0;
      uint32_t permissions = 0;
      if (vmm_query(&kernel_space, M13_BAR_BASE, &mapped_physical,
                    &permissions, &flags) != VMM_OK ||
          (mapped_physical & ~(PAGE_SIZE - 1)) != resource->base ||
          (flags & (UINT64_C(0x8) | UINT64_C(0x10))) !=
              (UINT64_C(0x8) | UINT64_C(0x10)) ||
          pci_unmap_bar(&mmio, M13_BAR_BASE, PAGE_SIZE) != PCI_OK)
        return 0;
      mapped_bar = 1;
      break;
    }
  }
  m13_release_mappings();
  serial("M13:PCI:PASS functions="); hex(function_count); serial("\n");
  serial("M13:DEVICE:PASS registered="); hex(m13_registry.count);
  serial(" mmio="); serial(mapped_bar ? "mapped\n" : "no-bar\n");
  return 1;
}
#if defined(LAMIX_M14_APIC)
#define M14_LAPIC_VA UINT64_C(0xfffff90000000000)
#define M14_IOAPIC_VA UINT64_C(0xfffff91000000000)
#define M14_PIT_DIVISOR 11931u
#if defined(LAMIX_M15_INPUT)
static uint32_t m14_io_read(volatile uint32_t *io, uint8_t reg);
static void m14_io_write(volatile uint32_t *io, uint8_t reg, uint32_t value);
typedef struct m15_route { size_t ioapic; uint32_t pin; uint8_t vector; } m15_route;
static int m15_route_isa(uint8_t irq, uint8_t vector, m15_route *route) {
  uint32_t gsi = irq;
  apic_irq_mode mode = {0, 0};
  for (size_t i = 0; i < m15_madt.override_count; ++i) {
    if (m15_madt.overrides[i].source == irq) {
      gsi = m15_madt.overrides[i].gsi;
      if (apic_decode_isa_override(m15_madt.overrides[i].flags, &mode) != APIC_OK)
        return 0;
      break;
    }
  }
  size_t owner = m15_madt.ioapic_count;
  for (size_t i = 0; i < m15_madt.ioapic_count; ++i) {
    uint32_t first = m15_madt.ioapics[i].gsi_base;
    uint32_t last = first + m14_ioapic_max[i];
    if (gsi >= first && gsi <= last) { owner = i; break; }
  }
  if (owner == m15_madt.ioapic_count) return 0;
  uint32_t pin = gsi - m15_madt.ioapics[owner].gsi_base;
  uint64_t redir;
  if (apic_make_redirection(vector, m15_bsp_id, mode, 1, &redir) != APIC_OK)
    return 0;
  uint8_t reg = (uint8_t)(0x10 + pin * 2);
  m14_io_write(m14_ioapics[owner], reg, (uint32_t)redir);
  m14_io_write(m14_ioapics[owner], (uint8_t)(reg + 1), (uint32_t)(redir >> 32));
  if (route) *route = (m15_route){owner, pin, vector};
  return 1;
}
static void m15_unmask_route(const m15_route *route) {
  uint8_t reg = (uint8_t)(0x10 + route->pin * 2);
  m14_io_write(m14_ioapics[route->ioapic], reg,
               m14_io_read(m14_ioapics[route->ioapic], reg) & ~(1u << 16));
}
static uint8_t m15_port_read(void *context, uint16_t port) {
  (void)context; return in8(port);
}
static void m15_port_write(void *context, uint16_t port, uint8_t value) {
  (void)context; out8(port, value);
}
static int m15_input_init(void) {
  m15_route keyboard, mouse;
  input_state_init(&m15_input);
  if (!m15_route_isa(1, 0x32, &keyboard)) {
    serial("M15:INIT:IRQ1_ROUTE\nM15:KEYBOARD:FAIL\nM15:FAIL\n"); return 0;
  }
  int mouse_route = m15_route_isa(12, 0x33, &mouse);
  serial("M15:INIT:PS2\n");
  ps2_status status = ps2_initialize(&m15_ps2,
      (ps2_io){0, m15_port_read, m15_port_write}, mouse_route);
  if (status != PS2_OK) {
    serial("M15:KEYBOARD:FAIL status="); hex(status); serial("\nM15:FAIL\n"); return 0;
  }
  m15_unmask_route(&keyboard);
  serial("M15:KEYBOARD:READY\n");
  if (mouse_route && m15_ps2.mouse_available) {
    m15_unmask_route(&mouse);
    serial("M15:MOUSE:READY\n");
  } else serial("M15:MOUSE:UNAVAILABLE\n");
  serial("M15:READY\n");
  __asm__ volatile("sti");
  uint8_t saw_plain_down = 0, saw_plain_up = 0, saw_shift_down = 0;
  uint8_t saw_shifted_down = 0, saw_shifted_up = 0, saw_shift_up = 0;
  uint8_t saw_mouse_x = 0, saw_mouse_y = 0, saw_mouse_button = 0;
  uint8_t saw_mouse_release = 0;
  uint64_t sequence_plain_down = 0, sequence_plain_up = 0;
  uint64_t sequence_shift_down = 0, sequence_shifted_down = 0;
  uint64_t sequence_shifted_up = 0, sequence_shift_up = 0;
  uint64_t last_sequence = 0;
  for (uint32_t spin = 0; spin < 500000000u; ++spin) {
    input_event event;
    while (input_poll(&m15_input, &event)) {
      if (event.sequence <= last_sequence) {
        serial("M15:QUEUE:FAIL:SEQUENCE\nM15:FAIL\n"); __asm__ volatile("cli"); return 0;
      }
      last_sequence = event.sequence;
      if (event.type == INPUT_EVENT_KEY) {
        if (event.data.key.keycode == INPUT_KEY_A && event.data.key.pressed && event.data.key.ascii == 'a' && !saw_plain_down) { saw_plain_down = 1; sequence_plain_down = event.sequence; }
        if (event.data.key.keycode == INPUT_KEY_A && !event.data.key.pressed && !event.data.key.ascii && saw_plain_down && !saw_plain_up) { saw_plain_up = 1; sequence_plain_up = event.sequence; }
        if (event.data.key.keycode == INPUT_KEY_LEFT_SHIFT && event.data.key.pressed && saw_plain_up && !saw_shift_down) { saw_shift_down = 1; sequence_shift_down = event.sequence; }
        if (event.data.key.keycode == INPUT_KEY_A && event.data.key.pressed && event.data.key.ascii == 'A' && (event.data.key.modifiers & INPUT_MOD_SHIFT) && saw_shift_down && !saw_shifted_down) { saw_shifted_down = 1; sequence_shifted_down = event.sequence; }
        if (event.data.key.keycode == INPUT_KEY_A && !event.data.key.pressed && saw_shifted_down && !saw_shifted_up) { saw_shifted_up = 1; sequence_shifted_up = event.sequence; }
        if (event.data.key.keycode == INPUT_KEY_LEFT_SHIFT && !event.data.key.pressed && saw_shifted_up && !saw_shift_up) { saw_shift_up = 1; sequence_shift_up = event.sequence; }
      } else if (event.type == INPUT_EVENT_MOUSE) {
        if (event.data.mouse.dx > 0) saw_mouse_x = 1;
        if (event.data.mouse.dy < 0) saw_mouse_y = 1;
        if (event.data.mouse.buttons & 1u) saw_mouse_button = 1;
        else if (saw_mouse_button) saw_mouse_release = 1;
      }
    }
    if (saw_plain_down && saw_plain_up && saw_shift_down && saw_shifted_down &&
        saw_shifted_up && saw_shift_up &&
        (!m15_ps2.mouse_available ||
         (saw_mouse_x && saw_mouse_y && saw_mouse_button && saw_mouse_release))) break;
    __asm__ volatile("pause");
  }
  __asm__ volatile("cli");
  input_stats stats; input_get_stats(&m15_input, &stats);
  if (!(saw_plain_down && saw_plain_up && saw_shift_down && saw_shifted_down &&
        saw_shifted_up && saw_shift_up)) {
    serial("M15:KEYBOARD:FAIL\nM15:FAIL\n"); return 0;
  }
  if (!(sequence_plain_down < sequence_plain_up &&
        sequence_plain_up < sequence_shift_down &&
        sequence_shift_down < sequence_shifted_down &&
        sequence_shifted_down < sequence_shifted_up &&
        sequence_shifted_up < sequence_shift_up)) {
    serial("M15:KEYBOARD:FAIL:ORDER\nM15:FAIL\n"); return 0;
  }
  serial("M15:KEYBOARD:PASS\n");
  if (m15_ps2.mouse_available && saw_mouse_x && saw_mouse_y && saw_mouse_button && saw_mouse_release) serial("M15:MOUSE:PASS\n");
  else if (m15_ps2.mouse_available) { serial("M15:MOUSE:FAIL\nM15:FAIL\n"); return 0; }
  else serial("M15:MOUSE:UNAVAILABLE\n");
  if (stats.dropped_events || stats.malformed_keyboard_bytes || stats.malformed_mouse_packets) {
    serial("M15:QUEUE:FAIL\nM15:FAIL\n"); return 0;
  }
  serial("M15:QUEUE:PASS\nM15:PASS\n");
  return 1;
}
#if defined(LAMIX_M17_VIRTIO_GPU)
static int m17_config_byte(pci_bdf bdf, uint16_t offset, uint8_t *value) {
  uint32_t word = 0;
  if (!value || !m13_config.read32(m13_config.context, bdf,
                                    (uint16_t)(offset & ~3u), &word))
    return 0;
  *value = (uint8_t)(word >> ((offset & 3u) * 8));
  return 1;
}
static int m17_config_dword(pci_bdf bdf, uint16_t offset, uint32_t *value) {
  return m13_config.read32(m13_config.context, bdf, offset, value);
}
static int m17_config_write_dword(pci_bdf bdf, uint16_t offset,
                                   uint32_t value) {
  return m13_config.write32(m13_config.context, bdf, offset, value);
}
static int m17_dma_address(void *context, const void *pointer, size_t length,
                           uint64_t *physical) {
  (void)context;
  if (!pointer || !length || !physical ||
      (uintptr_t)pointer > UINT64_MAX - length)
    return 0;
  uint64_t first = 0;
  size_t offset = 0;
  while (offset < length) {
    uint64_t at = (uintptr_t)pointer + offset, translated = 0, flags = 0;
    uint32_t permissions = 0;
    if (vmm_query(&kernel_space, at, &translated, &permissions, &flags) !=
        VMM_OK) {
      serial("M17:DMA:UNMAPPED at="); hex(at); serial("\n");
      return 0;
    }
    translated &= ~(PAGE_SIZE - 1);
    if (!offset) first = translated + (at & (PAGE_SIZE - 1));
    else if (translated != ((first + offset) & ~(PAGE_SIZE - 1))) {
      serial("M17:DMA:NONCONTIG at="); hex(at); serial("\n"); return 0;
    }
    size_t chunk = (size_t)(PAGE_SIZE - (at & (PAGE_SIZE - 1)));
    if (chunk > length - offset) chunk = length - offset;
    offset += chunk;
  }
  if (first >= PMM_PHYSICAL_LIMIT || length > PMM_PHYSICAL_LIMIT - first) {
    serial("M17:DMA:HIGH first="); hex(first); serial(" length=");
    hex(length); serial("\n");
    return 0;
  }
  *physical = first;
  return 1;
}
static uint32_t m17_read_common(void *context, uint16_t offset,
                                unsigned width) {
  m17_gpu_pci_context *pci = (m17_gpu_pci_context *)context;
  if (!pci || !pci->valid || offset + width > 56) return 0;
  if (width == 1) return *(volatile uint8_t *)(pci->common + offset);
  if (width == 2) return *(volatile uint16_t *)(void *)(pci->common + offset);
  if (width == 4) return *(volatile uint32_t *)(void *)(pci->common + offset);
  return 0;
}
static void m17_write_common(void *context, uint16_t offset, unsigned width,
                             uint32_t value) {
  m17_gpu_pci_context *pci = (m17_gpu_pci_context *)context;
  if (!pci || !pci->valid || offset + width > 56) return;
  if (width == 1) *(volatile uint8_t *)(pci->common + offset) = (uint8_t)value;
  else if (width == 2)
    *(volatile uint16_t *)(void *)(pci->common + offset) = (uint16_t)value;
  else if (width == 4)
    *(volatile uint32_t *)(void *)(pci->common + offset) = value;
  __asm__ volatile("mfence" ::: "memory");
}
static void m17_notify(void *context, uint16_t queue, uint16_t offset) {
  m17_gpu_pci_context *pci = (m17_gpu_pci_context *)context;
  if (!pci || !pci->valid) return;
  uint64_t byte_offset = (uint64_t)offset * pci->notify_multiplier;
  if (byte_offset + 2 > pci->notify_length) return;
  *(volatile uint16_t *)(void *)(pci->notify + byte_offset) = queue;
  __asm__ volatile("mfence" ::: "memory");
}

static int m17_find_device(void) {
  for (size_t i = 0; i < m13_registry.count; ++i) {
    pci_function *function = &m13_functions[i];
    if (function->vendor_id != 0x1af4 ||
        (function->device_id != 0x1050 && function->device_id != 0x1010) ||
        function->class_code != 0x03)
      continue;
    uint32_t command_status = 0;
    if (!m17_config_dword(function->bdf, 4, &command_status)) {
      serial("M17:PCI:CONFIG_STATUS\n"); return 0;
    }
    uint32_t capabilities = 0;
    if (!m17_config_dword(function->bdf, 4, &capabilities) ||
        !(capabilities & (UINT32_C(1) << 20))) {
      serial("M17:PCI:NO_CAP_LIST\n"); return 0;
    }
    uint16_t command = (uint16_t)command_status;
    if (!m17_config_write_dword(function->bdf, 4,
            (command_status & UINT32_C(0xffff0000)) | command | 6u))
      return 0;
    uint8_t cap = 0;
    if (!m17_config_byte(function->bdf, 0x34, &cap)) {
      serial("M17:PCI:CAP_PTR_READ\n"); return 0;
    }
    uint8_t common_bar = 0xff, notify_bar = 0xff;
    uint32_t common_offset = 0, common_length = 0;
    uint32_t notify_offset = 0, notify_length = 0, notify_multiplier = 0;
    for (unsigned step = 0; cap && step < 48; ++step) {
      if (cap < 0x40 || cap > 0xfc || (cap & 3)) return 0;
      uint8_t id = 0, next = 0;
      uint8_t length = 0, type = 0, bar = 0;
      uint32_t region = 0, region_size = 0, multiplier = 0;
      if (!m17_config_byte(function->bdf, cap, &id) ||
          !m17_config_byte(function->bdf, (uint16_t)(cap + 1), &next))
        return 0;
      if (id != 0x09) { cap = next; continue; }
      if (!m17_config_byte(function->bdf, (uint16_t)(cap + 2), &length) ||
          !m17_config_byte(function->bdf, (uint16_t)(cap + 3), &type) ||
          !m17_config_byte(function->bdf, (uint16_t)(cap + 4), &bar) ||
          !m17_config_dword(function->bdf, (uint16_t)(cap + 8), &region) ||
          !m17_config_dword(function->bdf, (uint16_t)(cap + 12),
                            &region_size)) return 0;
      uint32_t offset = region, size = region_size;
      if (type != 1 && type != 2) { cap = next; continue; }
      if (bar >= PCI_BAR_COUNT || !length || !size ||
          offset > UINT32_MAX - size ||
          !function->bars[bar].valid || function->bars[bar].is_io ||
          offset + size > function->bars[bar].size)
        { serial("M17:PCI:CAP:INVALID\n"); return 0; }
      if (type == 1 && length >= 16) {
        common_bar = bar; common_offset = offset; common_length = size;
      } else if (type == 2 && length >= 20) {
        if (!m17_config_dword(function->bdf, (uint16_t)(cap + 16),
                              &multiplier)) return 0;
        notify_bar = bar; notify_offset = offset; notify_length = size;
        notify_multiplier = multiplier;
      }
      cap = next;
    }
    if (common_bar == 0xff || notify_bar == 0xff || !notify_multiplier) {
      serial("M17:PCI:CAP:REQUIRED_MISSING\n"); return 0;
    }
    uint8_t bars_to_map[PCI_BAR_COUNT] = {0};
    bars_to_map[common_bar] = 1; bars_to_map[notify_bar] = 1;
    pci_mmio_ops mmio = {0, m13_map_bar, m13_unmap_bar};
    uint64_t common_va = 0, notify_va = 0;
    for (unsigned bar = 0; bar < PCI_BAR_COUNT; ++bar) {
      if (!bars_to_map[bar]) continue;
      uint64_t bar_va = M17_GPU_VA + (uint64_t)bar * M17_GPU_BAR_STRIDE;
      uint64_t map_length = function->bars[bar].size;
      if (!map_length || (map_length & (PAGE_SIZE - 1)) ||
          pci_map_bar(&mmio, function, bar, 0, bar_va, map_length) != PCI_OK) {
        serial("M17:PCI:BAR_MAP_FAIL bar="); hex(bar); serial(" size=");
        hex(map_length); serial(" base="); hex(function->bars[bar].base);
        serial("\n"); return 0;
      }
      if (bar == common_bar) common_va = bar_va + common_offset;
      if (bar == notify_bar) notify_va = bar_va + notify_offset;
    }
    if ((common_offset & 3) || common_length < 56 || notify_length < 2 ||
        common_va > UINT64_MAX - 56 || notify_va > UINT64_MAX - notify_length) {
      serial("M17:PCI:CAP_RANGE_FAIL\n"); return 0;
    }
    m17_pci_context = (m17_gpu_pci_context){
        (volatile uint8_t *)(uintptr_t)common_va,
        (volatile uint8_t *)(uintptr_t)notify_va,
        notify_multiplier, notify_length, 1};
    return 1;
  }
  return 0;
}

static int m17_init_gpu(uint32_t width, uint32_t height, uint32_t *pixels,
                        size_t bytes) {
  if (!m17_find_device()) { serial("M17:PCI:INIT_FAIL\n"); return 0; }
  uint64_t descriptors = 0, available = 0, used = 0;
  if (!m17_dma_address(0, m17_descriptors, sizeof(m17_descriptors),
                       &descriptors) ||
      !m17_dma_address(0, m17_available, sizeof(m17_available), &available) ||
      !m17_dma_address(0, m17_used, sizeof(m17_used), &used)) {
    serial("M17:PCI:DMA_QUEUE_FAIL\n"); return 0;
  }
  m17_memory = (virtio_gpu_memory){
      .descriptor_storage = m17_descriptors,
      .descriptor_storage_size = sizeof(m17_descriptors),
      .available_storage = m17_available,
      .available_storage_size = sizeof(m17_available),
      .used_storage = m17_used,
      .used_storage_size = sizeof(m17_used),
      .command_storage = m17_commands,
      .command_storage_size = sizeof(m17_commands),
      .backing_pages = 0,
      .backing_page_capacity = VIRTIO_GPU_MAX_BACKING_PAGES};
  m17_memory.queue.descriptors_physical = descriptors;
  m17_memory.queue.available_physical = available;
  m17_memory.queue.used_physical = used;
  virtio_gpu_ops ops = {&m17_pci_context, m17_read_common,
                        m17_write_common, m17_notify, m17_dma_address, 0};
  virtio_gpu_status status = virtio_gpu_initialize(
      &m17_gpu, &ops, &m17_memory, width, height, pixels, bytes);
  if (status != VIRTIO_GPU_OK) {
    serial("M17:TRANSPORT:FAIL status="); hex(status); serial("\n");
    if (status == VIRTIO_GPU_RESET_TIMEOUT) return -1;
    return 0;
  }
  return 1;
}
#endif
#endif
#if defined(LAMIX_M16_GRAPHICS)
#if defined(LAMIX_M17_VIRTIO_GPU)
static void m17_draw_frame(graphics_surface *surface, uint32_t width) {
  graphics_clear(surface, UINT32_C(0x00101828));
  graphics_fill_rect(surface, 24, 24, 192, 72, UINT32_C(0x003078d0));
  graphics_rect_outline(surface, 20, 20, 200, 80, UINT32_C(0x00ffffff));
  graphics_line(surface, 20, 110, (int32_t)width - 21, 110,
                UINT32_C(0x00ffb020));
  graphics_text(surface, 32, 48, "LAMIX OS M17", UINT32_C(0x00ffffff));
}

static int m17_renderer_init(const lbp_view *view) {
  const uint8_t *raw = record(view, LBP_FRAMEBUFFER);
  const lbp_framebuffer *fb = 0;
  if (raw && ((const lbp_record *)raw)->size >= sizeof(lbp_framebuffer)) {
    const lbp_framebuffer *candidate = (const lbp_framebuffer *)raw;
    if (candidate->bpp == 32 && candidate->physical &&
        candidate->physical <= UINTPTR_MAX && candidate->width &&
        candidate->height && candidate->pitch >= candidate->width * 4u &&
        (uint64_t)candidate->pitch * candidate->height <= candidate->length)
      fb = candidate;
  }
  uint32_t width = fb ? fb->width : 1024u;
  uint32_t height = fb ? fb->height : 768u;
  uint64_t bytes64 = (uint64_t)width * height * sizeof(uint32_t);
  if (!width || !height || bytes64 > SIZE_MAX ||
      bytes64 > M17_GPU_RESOURCE_LIMIT) {
    serial("M17:FRAMEBUFFER:UNAVAILABLE:SIZE\n");
    return 0;
  }
  size_t bytes = (size_t)bytes64;
  if (bytes > SIZE_MAX - 4095u) return 0;
  void *allocation = kheap_alloc(&kernel_heap, bytes + 4095u);
  if (!allocation) {
    serial("M17:FRAMEBUFFER:UNAVAILABLE:OOM\n");
    return 0;
  }
  uint32_t *back = (uint32_t *)(((uintptr_t)allocation + 4095u) &
                                ~(uintptr_t)4095u);
  graphics_surface surface;
  int gpu_result = m17_init_gpu(width, height, back, bytes);
  if (gpu_result < 0) {
    /* Device reset was not confirmed; retain DMA backing and stay serial-only. */
    serial("M17:GPU:FALLBACK:RESET_UNCONFIRMED\nM17:FAIL\n");
    return -1;
  }
  int gpu_ready = gpu_result > 0;
  if (gpu_ready) {
    width = m17_gpu.width;
    height = m17_gpu.height;
    size_t gpu_bytes = (size_t)width * height * sizeof(uint32_t);
    if (!graphics_init(&surface, back, bytes, (uint8_t *)back, gpu_bytes,
                       width, height, width * 4u, UINT32_C(0x00ff0000),
                       UINT32_C(0x0000ff00), UINT32_C(0x000000ff))) {
      int reset = virtio_gpu_shutdown(&m17_gpu);
      if (!reset) {
        serial("M17:GPU:RESET_UNCONFIRMED\nM17:FAIL\n");
        return -1;
      }
      gpu_ready = 0;
    }
  }
  if (!gpu_ready) {
    if (!fb || !graphics_init(&surface, back, bytes,
          (uint8_t *)(uintptr_t)fb->physical, fb->length, fb->width,
          fb->height, fb->pitch, fb->red_mask, fb->green_mask,
          fb->blue_mask)) {
      (void)kheap_free(&kernel_heap, allocation);
      serial("M17:GPU:FALLBACK:SERIAL_ONLY\nM17:UNAVAILABLE\n");
      return 0;
    }
    width = fb->width;
    serial("M17:GPU:FALLBACK:LBP_FRAMEBUFFER\n");
  } else {
    serial("M17:PCI:PASS\nM17:TRANSPORT:PASS\nM17:GPU:2D:PASS\n");
  }
  m17_draw_frame(&surface, width);
  uint64_t checksum = graphics_checksum(&surface);
  if (!checksum || !graphics_present(&surface) ||
      !graphics_verify_present(&surface) ||
      (gpu_ready && virtio_gpu_present(&m17_gpu, back,
          (size_t)surface.width * surface.height) != VIRTIO_GPU_OK)) {
    int reset = !gpu_ready || virtio_gpu_shutdown(&m17_gpu);
    if (reset) (void)kheap_free(&kernel_heap, allocation);
    else serial("M17:GPU:RESET_UNCONFIRMED; backing retained\n");
    serial("M17:FRAME:FAIL\nM17:FAIL\n");
    return -1;
  }
  if (gpu_ready) serial("M17:SCANOUT:PASS\n");
  serial("M17:FRAME:PASS checksum="); hex(checksum); serial("\n");
  serial("M16:RENDERER:PASS\nM16:FRAME:PASS checksum="); hex(checksum);
  serial("\nM17:READY\nM17:PASS\n");
  return 1;
}
#else
static int m16_renderer_init(const lbp_view *view) {
  const uint8_t *raw = record(view, LBP_FRAMEBUFFER);
  if (!raw || ((const lbp_record *)raw)->size < sizeof(lbp_framebuffer)) {
    serial("M16:FRAMEBUFFER:UNAVAILABLE:ABSENT\n");
    return 0;
  }
  const lbp_framebuffer *fb = (const lbp_framebuffer *)raw;
  if (fb->bpp != 32 || !fb->physical || fb->physical > UINTPTR_MAX ||
      (uint64_t)fb->width * fb->height > SIZE_MAX / sizeof(uint32_t)) {
    serial("M16:FRAMEBUFFER:UNAVAILABLE:INVALID\n");
    return 0;
  }
  size_t bytes = (size_t)fb->width * fb->height * sizeof(uint32_t);
  uint32_t *back = (uint32_t *)kheap_alloc(&kernel_heap, bytes);
  if (!back) {
    serial("M16:FRAMEBUFFER:UNAVAILABLE:OOM\n");
    return 0;
  }
  graphics_surface surface;
  if (!graphics_init(&surface, back, bytes,
                     (uint8_t *)(uintptr_t)fb->physical, fb->length,
                     fb->width, fb->height, fb->pitch, fb->red_mask,
                     fb->green_mask, fb->blue_mask)) {
    (void)kheap_free(&kernel_heap, back);
    serial("M16:FRAMEBUFFER:UNAVAILABLE:INVALID\n");
    return 0;
  }
  graphics_clear(&surface, UINT32_C(0x00101828));
  graphics_fill_rect(&surface, 24, 24, 192, 72, UINT32_C(0x003078d0));
  graphics_rect_outline(&surface, 20, 20, 200, 80, UINT32_C(0x00ffffff));
  graphics_line(&surface, 20, 110, (int32_t)fb->width - 21, 110,
                UINT32_C(0x00ffb020));
  graphics_text(&surface, 32, 48, "LAMIX OS M16", UINT32_C(0x00ffffff));
  uint64_t checksum = graphics_checksum(&surface);
  if (!checksum || !graphics_present(&surface) ||
      !graphics_verify_present(&surface)) {
    (void)kheap_free(&kernel_heap, back);
    serial("M16:FRAME:FAIL\n");
    return -1;
  }
  serial("M16:RENDERER:PASS\nM16:FRAME:PASS checksum=");
  hex(checksum);
  serial("\n");
  (void)kheap_free(&kernel_heap, back);
  return 1;
}
#endif
#endif
static uint32_t m14_io_read(volatile uint32_t *io, uint8_t reg) {
  io[0] = reg;
  __asm__ volatile("mfence" ::: "memory");
  return io[4];
}
static void m14_io_write(volatile uint32_t *io, uint8_t reg, uint32_t value) {
  io[0] = reg;
  __asm__ volatile("mfence" ::: "memory");
  io[4] = value;
  __asm__ volatile("mfence" ::: "memory");
}
static int m14_map_mmio(uint64_t virtual_address, uint64_t physical) {
  return physical < VMM_PHYSICAL_LIMIT && !(physical & (PAGE_SIZE - 1)) &&
         vmm_map_range(&kernel_space, virtual_address, physical, PAGE_SIZE,
                       VMM_READ | VMM_WRITE, VMM_MAP_UNCACHED) == VMM_OK;
}
static int m14_apic_init(void) {
  if (!m13_rsdp_size) { serial("M14:INIT:ACPI_ABSENT\n"); return 0; }
  acpi_platform platform = {0};
  acpi_status acs = acpi_discover(m13_rsdp, m13_rsdp_size, m13_read_physical, 0,
                                  m13_acpi_scratch, sizeof(m13_acpi_scratch),
                                  &platform);
  if (acs != ACPI_OK) { serial("M14:INIT:ACPI status="); hex(acs); serial("\n"); return 0; }
  if (!platform.madt_physical ||
      !m13_read_physical(0, platform.madt_physical, m13_acpi_scratch,
                         platform.madt_length)) {
    serial("M14:INIT:MADT_READ\n"); return 0;
  }
  uint32_t eax, ebx, ecx, edx;
  __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                   : "a"(1), "c"(0));
  if (!(edx & (1u << 9))) { serial("M14:INIT:NO_APIC\n"); return 0; }
  uint8_t bsp_id = (uint8_t)(ebx >> 24);
  acpi_madt_info info;
  acs = acpi_madt_parse(m13_acpi_scratch, platform.madt_length, bsp_id, &info);
  if (acs != ACPI_OK) { serial("M14:INIT:MADT status="); hex(acs); serial("\n"); return 0; }
#if defined(LAMIX_M15_INPUT)
  m15_madt = info;
  m15_bsp_id = bsp_id;
#endif
  uint32_t msr_low, msr_high;
  __asm__ volatile("rdmsr" : "=a"(msr_low), "=d"(msr_high) : "c"(0x1b));
  if (msr_low & (1u << 10)) { serial("M14:INIT:X2APIC\n"); return 0; }
  uint64_t apic_base = info.lapic_physical;
  if (apic_base >= VMM_PHYSICAL_LIMIT || (apic_base & 0xfff)) return 0;
  uint64_t current_base = ((uint64_t)(msr_high & 0xfu) << 32) |
                          (msr_low & UINT32_C(0xfffff000));
  if (current_base != apic_base) { serial("M14:INIT:APIC_BASE\n"); return 0; }
  __asm__ volatile("wrmsr" :: "a"(msr_low | (1u << 11)), "d"(msr_high),
                   "c"(0x1b) : "memory");
  if (!m14_map_mmio(M14_LAPIC_VA, apic_base)) { serial("M14:INIT:LAPIC_MAP\n"); return 0; }
  m14_lapic = (volatile uint32_t *)(uintptr_t)M14_LAPIC_VA;
  m14_lapic[0xf0 / 4] = 0x1ff;
  m14_lapic[0x80 / 4] = 0;
  m14_lapic[0x350 / 4] = 1u << 16;
  m14_lapic[0x360 / 4] = 1u << 16;
  for (size_t i = 0; i < info.ioapic_count; ++i) {
    uint64_t va = M14_IOAPIC_VA + i * PAGE_SIZE;
    if (!m14_map_mmio(va, info.ioapics[i].physical)) { serial("M14:INIT:IOAPIC_MAP\n"); return 0; }
    volatile uint32_t *io = (volatile uint32_t *)(uintptr_t)va;
    m14_ioapics[i] = io;
    uint32_t version = m14_io_read(io, 1);
    uint32_t max = (version >> 16) & 0xff;
    if (max > 119 || info.ioapics[i].gsi_base > UINT32_MAX - max)
      { serial("M14:INIT:IOAPIC_RANGE\n"); return 0; }
    m14_ioapic_max[i] = max;
    for (uint32_t pin = 0; pin <= max; ++pin) {
      uint8_t reg = (uint8_t)(0x10 + pin * 2);
      m14_io_write(io, reg, 1u << 16);
      m14_io_write(io, (uint8_t)(reg + 1), 0);
    }
    uint32_t end = info.ioapics[i].gsi_base + max;
    for (size_t j = 0; j < i; ++j) {
      uint32_t other_start = info.ioapics[j].gsi_base;
      uint32_t other_end = other_start + m14_ioapic_max[j];
      if (info.ioapics[i].gsi_base <= other_end && other_start <= end)
        { serial("M14:INIT:GSI_OVERLAP\n"); return 0; }
    }
  }
  uint32_t gsi = 0;
  apic_irq_mode mode = {0, 0};
  for (size_t i = 0; i < info.override_count; ++i) {
    if (info.overrides[i].source == 0) {
      gsi = info.overrides[i].gsi;
      if (apic_decode_isa_override(info.overrides[i].flags, &mode) != APIC_OK)
        { serial("M14:INIT:ISO_FLAGS\n"); return 0; }
    }
  }
  size_t owner = info.ioapic_count;
  uint32_t pin = 0;
  for (size_t i = 0; i < info.ioapic_count; ++i) {
    uint32_t first = info.ioapics[i].gsi_base;
    uint32_t last = first + m14_ioapic_max[i];
    if (gsi >= first && gsi <= last) { owner = i; pin = gsi - first; break; }
  }
  if (owner == info.ioapic_count) { serial("M14:INIT:GSI_UNROUTED\n"); return 0; }
  uint64_t redirection;
  if (apic_make_redirection(0x30, bsp_id, mode, 0, &redirection) != APIC_OK)
    { serial("M14:INIT:REDIRECTION\n"); return 0; }
  volatile uint32_t *io = m14_ioapics[owner];
  uint8_t redir_reg = (uint8_t)(0x10 + pin * 2);
  m14_io_write(io, redir_reg, (uint32_t)redirection | (1u << 16));
  m14_io_write(io, (uint8_t)(redir_reg + 1), (uint32_t)(redirection >> 32));
  m14_lapic[0x320 / 4] = (1u << 16) | 0x31;
  m14_lapic[0x3e0 / 4] = 0x3;
  out8(0x21, 0xff); out8(0xa1, 0xff);
  out8(PIT + 3, 0x36);
  out8(PIT, (uint8_t)M14_PIT_DIVISOR);
  out8(PIT, (uint8_t)(M14_PIT_DIVISOR >> 8));
  m14_io_write(io, redir_reg, (uint32_t)redirection);
  m14_active = 1;
  serial("M14:ACPI:PASS\nM14:APIC:PASS\nM14:IOAPIC:PASS\n");
  m14_lapic[0x380 / 4] = UINT32_MAX;
  __asm__ volatile("sti");
  for (uint32_t n = 0; n < 20000000u &&
       m14_pit_irqs < M14_PIT_CALIBRATION_IRQS; ++n)
    __asm__ volatile("pause");
  __asm__ volatile("cli");
  if (m14_pit_irqs < M14_PIT_CALIBRATION_IRQS) {
    serial("M14:INIT:PIT_TIMEOUT gsi="); hex(gsi);
    serial(" mode="); hex((mode.active_low ? 1u : 0u) |
                           (mode.level_triggered ? 2u : 0u));
    serial(" redir-low="); hex(m14_io_read(io, redir_reg));
    serial(" redir-high="); hex(m14_io_read(io, (uint8_t)(redir_reg + 1)));
    serial(" lapic-irr="); hex(m14_lapic[0x210 / 4]);
    serial(" lapic-isr="); hex(m14_lapic[0x110 / 4]);
    serial("\n");
    return 0;
  }
  m14_io_write(io, redir_reg, (uint32_t)redirection | (1u << 16));
  uint32_t elapsed;
  if (m14_pit_eois != m14_pit_irqs ||
      apic_elapsed_counts(m14_calibration_start, m14_calibration_end,
                          &elapsed) != APIC_OK) {
    serial("M14:INIT:PIT_EOI_OR_SAMPLE\n"); return 0;
  }
  serial("M14:PIT_REF:PASS\n");
  uint32_t periodic;
  if (apic_calibrate_periodic(elapsed, M14_PIT_CALIBRATION_INTERVALS,
                              M14_PIT_DIVISOR,
                              100, &periodic) != APIC_OK)
    { serial("M14:INIT:CALIBRATION\n"); return 0; }
  m14_lapic[0x320 / 4] = (1u << 17) | 0x31;
  m14_lapic[0x380 / 4] = periodic;
  __asm__ volatile("sti");
  for (uint32_t n = 0; n < 20000000u && m14_timer_irqs < 10; ++n)
    __asm__ volatile("pause");
  __asm__ volatile("cli");
  if (m14_timer_irqs < 10 || m14_timer_eois != m14_timer_irqs ||
      in8(0x21) != 0xff || in8(0xa1) != 0xff) { serial("M14:INIT:TIMER_TIMEOUT_OR_EOI\n"); return 0; }
  serial("M14:LAPIC_TIMER:PASS\n");
  return 1;
}
#endif
#endif
#if defined(LAMIX_M10_VMM)
static int vmm_allocate(void *context, uint64_t *physical) {
  (void)context;
  return kernel_alloc_frame(physical);
}
static int vmm_release(void *context, uint64_t physical) {
  (void)context;
  return kernel_free_frame(physical);
}
static volatile uint64_t *vmm_access(void *context, uint64_t physical) {
  (void)context;
  if (physical >= VMM_PHYSICAL_LIMIT || (physical & (PAGE_SIZE - 1)))
    return 0;
  if (!vmm_scratch_active) {
    uint64_t root;
    __asm__ volatile("mov %%cr3,%0" : "=r"(root));
    volatile uint64_t *pte = page_leaf(root, VMM_SCRATCH_BASE,
                                       vmm_scratch_base,
                                       vmm_scratch_table_bytes, 1);
    if (!pte)
      return 0;
    *pte = physical | PTE_PRESENT | PTE_WRITE |
           (vmm_nx_enabled ? PTE_NX : 0);
    invalidate(VMM_SCRATCH_BASE);
    return (volatile uint64_t *)(uintptr_t)VMM_SCRATCH_BASE;
  }
  volatile uint64_t *control =
      (volatile uint64_t *)(uintptr_t)vmm_scratch_control;
  unsigned index = (unsigned)((VMM_SCRATCH_BASE >> 12) & 511);
  control[index] = physical | PTE_PRESENT | PTE_WRITE |
                   (vmm_nx_enabled ? PTE_NX : 0);
  invalidate(VMM_SCRATCH_BASE);
  return (volatile uint64_t *)(uintptr_t)VMM_SCRATCH_BASE;
}
static void vmm_invalidate(void *context, uint64_t virtual_address) {
  (void)context;
  invalidate(virtual_address);
}
static int vmm_map_bootinfo(vmm_space *space, const lbp_header *header) {
  const uint8_t *raw = raw_record(header, LBP_RESERVATIONS);
  if (!raw || ((const lbp_record *)raw)->size < 32)
    return 0;
  uint32_t stride = *(const uint32_t *)(raw + 20);
  uint64_t count = *(const uint64_t *)(raw + 24);
  uint64_t size = ((const lbp_record *)raw)->size;
  if (stride < sizeof(lbp_reservation) || (stride & 7) ||
      count > (size - 32) / stride)
    return 0;
  for (uint64_t i = 0; i < count; ++i) {
    const uint8_t *entry = raw + 32 + i * stride;
    if (*(const uint32_t *)(entry + 16) == LBP_PURPOSE_BOOTINFO) {
      uint64_t base = *(const uint64_t *)entry;
      uint64_t length = *(const uint64_t *)(entry + 8);
      return base == header->self_phys &&
             vmm_map_range(space, base, base, length, VMM_READ, 0) == VMM_OK;
    }
  }
  return 0;
}
static int vmm_build_root(const lbp_view *view, const lbp_header *header,
                          const lbp_entry *entry) {
  uint64_t table_base = 0, table_size = 0;
  if (!table_reservation(header, &table_base, &table_size))
    return 0;
  vmm_scratch_base = table_base;
  vmm_scratch_table_bytes = table_size;
  uint8_t nx_enabled = !!(header->provided_features & LBP_FEATURE_NX);
  vmm_nx_enabled = nx_enabled;
  vmm_operations operations = {.context = 0,
                               .allocate_page = vmm_allocate,
                               .release_page = vmm_release,
                               .access_page = vmm_access,
                               .invalidate = vmm_invalidate};
  if (vmm_init(&kernel_space, &operations, nx_enabled) != VMM_OK)
    return 0;

  const uint8_t *raw_kernel = record(view, LBP_KERNEL);
  if (!raw_kernel || ((const lbp_record *)raw_kernel)->size < 40)
    return 0;
  uint64_t size = ((const lbp_record *)raw_kernel)->size;
  uint32_t stride = *(const uint32_t *)(raw_kernel + 28);
  uint64_t count = *(const uint64_t *)(raw_kernel + 32);
  if (stride < sizeof(lbp_segment) || (stride & 7) ||
      count > (size - 40) / stride || count > 32)
    return 0;
  for (uint64_t i = 0; i < count; ++i) {
    const lbp_segment *segment =
        (const lbp_segment *)(raw_kernel + 40 + i * stride);
    if (vmm_map_range(&kernel_space, segment->virtual_address,
                      segment->physical, segment->length,
                      segment->permissions, 0) != VMM_OK)
      return 0;
  }
  if (!vmm_map_bootinfo(&kernel_space, header) ||
      entry->stack_size < 65536 || (entry->stack_physical & (PAGE_SIZE - 1)) ||
      (entry->stack_size & (PAGE_SIZE - 1)) ||
      entry->guard > UINT64_MAX - PAGE_SIZE ||
      entry->guard + PAGE_SIZE != entry->stack_physical ||
      vmm_map_range(&kernel_space, entry->stack_physical,
                    entry->stack_physical, entry->stack_size,
                    VMM_READ | VMM_WRITE, 0) != VMM_OK)
    return 0;

  const uint8_t *raw_fb = record(view, LBP_FRAMEBUFFER);
  if (raw_fb) {
    if (((const lbp_record *)raw_fb)->size < sizeof(lbp_framebuffer))
      return 0;
    const lbp_framebuffer *fb = (const lbp_framebuffer *)raw_fb;
    if (fb->length) {
      uint64_t end = fb->physical + fb->length;
      if (end < fb->physical || end > VMM_PHYSICAL_LIMIT - (PAGE_SIZE - 1))
        return 0;
      uint64_t first = fb->physical & ~(PAGE_SIZE - 1);
      uint64_t last = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
      if (vmm_map_range(&kernel_space, first, first, last - first,
                        VMM_READ | VMM_WRITE, VMM_MAP_UNCACHED) != VMM_OK)
        return 0;
    }
  }

  uint64_t physical = 0, flags = 0;
  uint32_t permissions = 0;
  if (vmm_query(&kernel_space, entry->guard, &physical, &permissions, &flags) !=
      VMM_NOT_MAPPED)
    return 0;
  if (vmm_prepare_scratch(&kernel_space, VMM_SCRATCH_BASE,
                          VMM_SCRATCH_CONTROL,
                          &vmm_scratch_control_page) != VMM_OK)
    return 0;
  vmm_scratch_control = VMM_SCRATCH_CONTROL;
  vmm_scratch_active = 0;
  __asm__ volatile("mov %0,%%cr3" : : "r"(kernel_space.root_physical) : "memory");
  vmm_scratch_active = 1;

  for (uint64_t i = 0; i < count; ++i) {
    const lbp_segment *segment =
        (const lbp_segment *)(raw_kernel + 40 + i * stride);
    uint32_t expected_permissions = segment->permissions;
    if (!kernel_space.nx_enabled && !(expected_permissions & VMM_WRITE))
      expected_permissions |= VMM_EXECUTE;
    if (vmm_query(&kernel_space, segment->virtual_address, &physical,
                  &permissions, &flags) != VMM_OK ||
        physical != segment->physical || permissions != expected_permissions)
      return 0;
  }
  if (vmm_query(&kernel_space, entry->rsp - 8, &physical, &permissions,
                &flags) != VMM_OK || physical != entry->rsp - 8 ||
      !(permissions & VMM_WRITE) ||
      vmm_query(&kernel_space, entry->guard, &physical, &permissions,
                &flags) != VMM_NOT_MAPPED ||
      ((const lbp_header *)(uintptr_t)header->self_phys)->self_phys !=
          header->self_phys)
    return 0;
  return 1;
}
static int vmm_self_test(void) {
  if (!kernel_alloc_frame(&vmm_test_physical) ||
      vmm_map_range(&kernel_space, VMM_TEST_BASE, vmm_test_physical,
                    PAGE_SIZE, VMM_READ | VMM_WRITE, 0) != VMM_OK)
    return 0;
  volatile uint64_t *probe = (volatile uint64_t *)(uintptr_t)VMM_TEST_BASE;
  *probe = UINT64_C(0x4d3130564d4d);
  if (*probe != UINT64_C(0x4d3130564d4d) ||
      vmm_protect_range(&kernel_space, VMM_TEST_BASE, PAGE_SIZE, VMM_READ) !=
          VMM_OK)
    return 0;
  uint64_t physical = 0, flags = 0;
  uint32_t permissions = 0;
  if (vmm_query(&kernel_space, VMM_TEST_BASE, &physical, &permissions,
                &flags) != VMM_OK || physical != vmm_test_physical ||
      permissions != (kernel_space.nx_enabled ? VMM_READ
                                               : VMM_READ | VMM_EXECUTE) ||
      (kernel_space.nx_enabled && !(flags & PTE_NX)) ||
      vmm_unmap_range(&kernel_space, VMM_TEST_BASE, PAGE_SIZE) != VMM_OK ||
      vmm_query(&kernel_space, VMM_TEST_BASE, &physical, &permissions,
                &flags) != VMM_NOT_MAPPED ||
      !kernel_free_frame(vmm_test_physical))
    return 0;
  serial("M10:VMM root=switched NX=");
  serial(kernel_space.nx_enabled ? "1\n" : "0\n");
  return 1;
}
#if defined(LAMIX_M11_HEAP)
static int heap_map_page(void *context, uint64_t virtual_address,
                         void **accessible_page) {
  (void)context;
  uint64_t physical = 0;
  if (!accessible_page || !kernel_alloc_frame(&physical))
    return 0;
  if (vmm_map_range(&kernel_space, virtual_address, physical, PAGE_SIZE,
                    VMM_READ | VMM_WRITE, 0) != VMM_OK) {
    (void)kernel_free_frame(physical);
    return 0;
  }
  *accessible_page = (void *)(uintptr_t)virtual_address;
  return 1;
}

static int heap_release_page(void *context, uint64_t virtual_address) {
  (void)context;
  uint64_t physical = 0, flags = 0;
  uint32_t permissions = 0;
  if (vmm_query(&kernel_space, virtual_address, &physical, &permissions,
                &flags) != VMM_OK ||
      vmm_unmap_range(&kernel_space, virtual_address, PAGE_SIZE) != VMM_OK)
    return 0;
  physical &= ~(PAGE_SIZE - 1);
  return kernel_free_frame(physical);
}

static int heap_self_test(void) {
  kheap_operations operations = {.context = 0,
                                 .map_page = heap_map_page,
                                 .release_page = heap_release_page};
  if (kheap_init(&kernel_heap, &operations, KHEAP_VIRTUAL_BASE,
                 KHEAP_VIRTUAL_SIZE) != KHEAP_OK ||
      kheap_alloc(&kernel_heap, 0) != 0 ||
      kernel_heap.last_error != KHEAP_INVALID_ARGUMENT)
    return 0;
  uint8_t *first = kheap_alloc(&kernel_heap, 13);
  uint8_t *zeroed = kheap_calloc(&kernel_heap, 64, 3);
  uint8_t *large = kheap_alloc(&kernel_heap, 5000);
  if (!first || !zeroed || !large || ((uintptr_t)first & 15) ||
      ((uintptr_t)zeroed & 15) || ((uintptr_t)large & 15))
    return 0;
  for (unsigned i = 0; i < 192; ++i)
    if (zeroed[i])
      return 0;
  first[0] = 0x5a;
  large[0] = 0xa5;
  if (!m11_cpp_heap_probe(&kernel_heap) || first[0] != 0x5a ||
      large[0] != 0xa5 ||
      kheap_free(&kernel_heap, first) != KHEAP_OK ||
      kheap_free(&kernel_heap, zeroed) != KHEAP_OK ||
      kheap_free(&kernel_heap, large) != KHEAP_OK ||
      kheap_free(&kernel_heap, first) != KHEAP_DOUBLE_FREE ||
      kheap_free(&kernel_heap,
                 (void *)(uintptr_t)(KHEAP_VIRTUAL_BASE + 16)) !=
          KHEAP_INVALID_POINTER)
    return 0;
  kheap_stats stats;
  if (kheap_get_stats(&kernel_heap, &stats) != KHEAP_OK ||
      stats.live_allocations != 0 || stats.live_bytes != 0 ||
      stats.allocation_count != 4 || stats.free_count != 4 ||
      stats.double_free_count != 1 || stats.invalid_free_count != 1 ||
      !stats.mapped_pages)
    return 0;
  serial("M11:HEAP pages=");
  hex(stats.mapped_pages);
  serial(" peak=");
  hex(stats.peak_live_bytes);
  serial("\n");
  return 1;
}
#endif
static __attribute__((noreturn, unused)) void
vmm_fault_test(const lbp_entry *entry) {
  (void)entry;
#if defined(LAMIX_M10_FAULT_RO)
  if (pmm_alloc_frame(&physical_memory, &vmm_test_physical) != PMM_OK ||
      vmm_map_range(&kernel_space, VMM_TEST_BASE, vmm_test_physical,
                    PAGE_SIZE, VMM_READ, 0) != VMM_OK)
    fail();
  serial("M10:FAULT_READY ro\n");
  *(volatile uint8_t *)(uintptr_t)VMM_TEST_BASE = 0x5a;
#elif defined(LAMIX_M10_FAULT_NX)
  if (!kernel_space.nx_enabled ||
      pmm_alloc_frame(&physical_memory, &vmm_test_physical) != PMM_OK ||
      vmm_map_range(&kernel_space, VMM_TEST_BASE, vmm_test_physical,
                    PAGE_SIZE, VMM_READ, 0) != VMM_OK)
    fail();
  serial("M10:FAULT_READY nx\n");
  ((void (*)(void))(uintptr_t)VMM_TEST_BASE)();
#elif defined(LAMIX_M10_FAULT_GUARD)
  serial("M10:FAULT_READY guard\n");
  *(volatile uint8_t *)(uintptr_t)entry->guard = 0x5a;
#else
#endif
  halt();
}
#endif
static __attribute__((noreturn)) void fail(void){serial("M08:FAIL\n");halt();}
__attribute__((noreturn)) void kernel_main(uint64_t self,uint64_t magic){
  serial_init();serial("M08:ENTRY\n");if(magic!=LBP_ENTRY_MAGIC||!self)fail();const lbp_header*h=(const lbp_header*)(uintptr_t)self;lbp_view view;
  if(h->self_phys!=self||__builtin_memcmp(h->magic,LBP_MAGIC,8))fail();
  const uint8_t*raw_acpi=raw_record(h,LBP_ACPI);lbp_span span,*spans=0;uint64_t span_count=0;
  if(raw_acpi){if(((const lbp_record*)raw_acpi)->size<sizeof(lbp_acpi)||!map_acpi_span(h,(const lbp_acpi*)raw_acpi,&span))fail();spans=&span;span_count=1;}
#if defined(LAMIX_M13_DEVICES)
  if (raw_acpi && span.size <= sizeof(m13_rsdp)) {
    m13_rsdp_size = (size_t)span.size;
    for (size_t i = 0; i < m13_rsdp_size; ++i)
      m13_rsdp[i] = span.bytes[i];
  }
#endif
  lbp_status validation=lbp_validate(h,h->total_size,spans,span_count,&view);
  if(raw_acpi)unmap_acpi_span();
  if(validation!=LBP_OK)fail();
#if defined(LAMIX_M14_APIC)
  if (!(h->provided_features & LBP_FEATURE_ACPI) || !raw_acpi) {
    serial("M14:ACPI:FAIL\nM14:FAIL\n");
    fail();
  }
#endif
  const lbp_entry*e=(const lbp_entry*)record(&view,LBP_ENTRY);
  if(!e)fail();
#if defined(LAMIX_M09_PMM)
  if(!pmm_self_test(&view,e)){serial("M09:FAIL\n");fail();}
#endif
#if defined(LAMIX_M12_BUDDY)
  if (!pmm_buddy_self_test(&view)) {
    serial("M12:FAIL\n");
    fail();
  }
#endif
  uint16_t prior_cs;__asm__ volatile("mov %%cs,%0":"=r"(prior_cs));
  idt_init(prior_cs,0);
  gdt_init(prior_cs);
  idt_init(KCODE,1);
#if defined(LAMIX_M10_VMM)
  if (!vmm_build_root(&view, h, e) || !vmm_self_test()) {
    serial("M10:FAIL\n");
    fail();
  }
#endif
#if defined(LAMIX_M11_HEAP)
  if (!heap_self_test()) {
    serial("M11:FAIL\n");
    fail();
  }
#endif
#if defined(LAMIX_M13_DEVICES)
  if (!m13_platform_init()) {
    serial("M13:FAIL\n");
    fail();
  }
#endif
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
#if defined(LAMIX_M10_VMM)
#if defined(LAMIX_M10_FAULT_RO) || defined(LAMIX_M10_FAULT_NX) || \
    defined(LAMIX_M10_FAULT_GUARD)
  vmm_fault_test(e);
#else
  serial("M10:PASS\n");
#endif
#endif
#if defined(LAMIX_M11_HEAP)
  serial("M11:PASS\n");
#endif
#if defined(LAMIX_M12_BUDDY)
  serial("M12:PASS\n");
#endif
#if defined(LAMIX_M13_DEVICES)
  serial("M13:PASS\n");
#endif
#if defined(LAMIX_M14_APIC)
  if (!m14_apic_init()) {
    serial("M14:FAIL\n");
    fail();
  }
  serial("M14:PASS\n");
#endif
#if defined(LAMIX_M15_INPUT)
  if (!m15_input_init()) halt();
#endif
#if defined(LAMIX_M16_GRAPHICS)
#if defined(LAMIX_M17_VIRTIO_GPU)
  int graphics_status = m17_renderer_init(&view);
#else
  int graphics_status = m16_renderer_init(&view);
#endif
  if (graphics_status < 0) {
#if defined(LAMIX_M17_VIRTIO_GPU)
    serial("M17:FAIL\n");
#else
    serial("M16:FAIL\n");
#endif
    fail();
  }
  if (!graphics_status) {
#if defined(LAMIX_M17_VIRTIO_GPU)
    serial("M17:UNAVAILABLE\n");
#else
    serial("M16:UNAVAILABLE\n");
#endif
    halt();
  }
#if !defined(LAMIX_M17_VIRTIO_GPU)
  serial("M16:PASS\n");
#endif
#endif
  halt();
}
