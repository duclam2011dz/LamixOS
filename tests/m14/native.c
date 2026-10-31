/* SPDX-License-Identifier: GPL-3.0-only */
#include "acpi.h"
#include "apic.h"
#include "lamix/boot/acpi_rsdp.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t memory[0x4000];
static void put32(uint8_t *p, uint32_t v) {
  p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static void put64(uint8_t *p, uint64_t v) { put32(p,(uint32_t)v); put32(p+4,(uint32_t)(v>>32)); }
static void checksum(uint8_t *p, size_t n, size_t at) {
  uint8_t s=0; p[at]=0; for(size_t i=0;i<n;i++) s=(uint8_t)(s+p[i]); p[at]=(uint8_t)(0-s);
}
static int read_phys(void *context, uint64_t address, void *destination, size_t n) {
  (void)context;
  if (address > sizeof(memory) || n > sizeof(memory)-(size_t)address) return 0;
  memcpy(destination,memory+(size_t)address,n); return 1;
}
static void test_rsdp(void) {
  uint8_t rsdp[48] __attribute__((aligned(16)))={0};
  memcpy(rsdp,"RSD PTR ",8); rsdp[15]=2; put32(rsdp+20,36); put64(rsdp+24,0x1000);
  checksum(rsdp,20,8); checksum(rsdp,36,32);
  uint32_t size=0,revision=0;
  assert(lamix_rsdp_validate(rsdp,36,&size,&revision) && size==36 && revision==2);
  rsdp[9]^=1; assert(!lamix_rsdp_validate(rsdp,36,&size,&revision)); rsdp[9]^=1;
  rsdp[15]=0; checksum(rsdp,20,8);
  assert(lamix_rsdp_validate(rsdp,20,&size,&revision) && size==20 && revision==0);
  assert(!lamix_rsdp_validate(rsdp+1,20,&size,&revision));
  rsdp[15]=1; assert(!lamix_rsdp_validate(rsdp,36,&size,&revision));
}
static void test_madt(void) {
  uint8_t madt[44+8+12+10]={0};
  memcpy(madt,"APIC",4); put32(madt+4,sizeof(madt)); put32(madt+36,0xfee00000);
  size_t p=44; madt[p]=0; madt[p+1]=8; madt[p+3]=2; put32(madt+p+4,1); p+=8;
  madt[p]=1; madt[p+1]=12; madt[p+2]=3; put32(madt+p+4,0xfec00000); put32(madt+p+8,0); p+=12;
  madt[p]=2; madt[p+1]=10; madt[p+3]=0; put32(madt+p+4,2); madt[p+8]=0x0f;
  checksum(madt,sizeof(madt),9);
  acpi_madt_info info;
  assert(acpi_madt_parse(madt,sizeof(madt),2,&info)==ACPI_OK);
  assert(info.lapic_physical==0xfee00000 && info.ioapic_count==1 &&
         info.ioapics[0].physical==0xfec00000 && info.override_count==1 &&
         info.overrides[0].gsi==2 && info.bsp_present);
  uint8_t saved=madt[sizeof(madt)-1]; madt[sizeof(madt)-1]^=1;
  assert(acpi_madt_parse(madt,sizeof(madt),2,&info)!=ACPI_OK);
  madt[sizeof(madt)-1]=saved;
  assert(acpi_madt_parse(madt,sizeof(madt)-1,2,&info)!=ACPI_OK);
  uint8_t invalid_flags[sizeof(madt)]; memcpy(invalid_flags,madt,sizeof(madt));
  invalid_flags[sizeof(madt)-1]=1; checksum(invalid_flags,sizeof(invalid_flags),9);
  assert(acpi_madt_parse(invalid_flags,sizeof(invalid_flags),2,&info)==ACPI_BAD_ENTRY);
  uint8_t duplicate[sizeof(madt)+10]; memcpy(duplicate,madt,sizeof(madt));
  duplicate[sizeof(madt)]=2; duplicate[sizeof(madt)+1]=10;
  duplicate[sizeof(madt)+3]=0; put32(duplicate+sizeof(madt)+4,5);
  put32(duplicate+4,sizeof(duplicate)); checksum(duplicate,sizeof(duplicate),9);
  assert(acpi_madt_parse(duplicate,sizeof(duplicate),2,&info)==ACPI_BAD_ENTRY);
  put32(madt+36,0xfee00001); checksum(madt,sizeof(madt),9);
  assert(acpi_madt_parse(madt,sizeof(madt),2,&info)==ACPI_BAD_ADDRESS);
}
static void test_unaligned_root_is_bounded(void) {
  memset(memory,0,sizeof(memory));
  uint8_t *rsdp=memory+0x100, *rsdt=memory+0x1003;
  memcpy(rsdp,"RSD PTR ",8); rsdp[15]=0; put32(rsdp+16,0x1003);
  checksum(rsdp,20,8);
  memcpy(rsdt,"RSDT",4); put32(rsdt+4,36); rsdt[8]=1;
  checksum(rsdt,36,9);
  uint8_t scratch[128]; acpi_platform platform={0};
  assert(acpi_discover(rsdp,20,read_phys,0,scratch,sizeof(scratch),&platform)==ACPI_OK);
  assert(platform.root.physical==0x1003 && !platform.root.xsdt);
}
static void test_apic_helpers(void) {
  apic_irq_mode mode; uint64_t redirection=0; uint32_t count=0;
  assert(apic_decode_isa_override(0x0f,&mode)==APIC_OK &&
         mode.active_low && mode.level_triggered);
  assert(apic_decode_isa_override(2,&mode)==APIC_INVALID);
  assert(apic_make_redirection(0x30,2,mode,1,&redirection)==APIC_OK);
  assert((redirection&0xff)==0x30 && ((redirection>>56)&0xff)==2 &&
         (redirection&(1u<<13)) && (redirection&(1u<<15)) &&
         (redirection&(1u<<16)));
  assert(apic_make_redirection(0xff,2,mode,0,&redirection)==APIC_INVALID);
  assert(apic_calibrate_periodic(62500000,10,11931,100,&count)==APIC_OK &&
         count>6000000 && count<6500000);
  assert(apic_calibrate_periodic(UINT32_MAX,0,11931,100,&count)==APIC_INVALID);
  assert(apic_calibrate_periodic(1,10,11931,100,&count)==APIC_RANGE);
}
int main(void) { test_rsdp(); test_madt(); test_unaligned_root_is_bounded(); test_apic_helpers(); return 0; }
