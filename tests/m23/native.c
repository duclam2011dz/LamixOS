/* SPDX-License-Identifier: GPL-3.0-only */
#include "elf_loader.h"
#include "initramfs.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int allocations;
static int fail_alloc;
static void *tracked_alloc(void *context, size_t size) {
  (void)context;
  if (fail_alloc) return 0;
  void *p = malloc(size);
  if (p) ++allocations;
  return p;
}
static void tracked_free(void *context, void *p) {
  (void)context;
  if (p) --allocations;
  free(p);
}
static void w16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8); }
static void w32(uint8_t *p, uint32_t v) { for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8)); }
static void w64(uint8_t *p, uint64_t v) { for(int i=0;i<8;i++)p[i]=(uint8_t)(v>>(i*8)); }
static void phdr(uint8_t *p,uint32_t type,uint32_t flags,uint64_t off,
                 uint64_t va,uint64_t filesz,uint64_t memsz) {
  w32(p,type);w32(p+4,flags);w64(p+8,off);w64(p+16,va);w64(p+24,va);
  w64(p+32,filesz);w64(p+40,memsz);w64(p+48,4096);
}
static void dynamic(uint8_t *p,int bad_type) {
  w64(p,7);w64(p+8,0x1050);       /* DT_RELA */
  w64(p+16,8);w64(p+24,24);       /* DT_RELASZ */
  w64(p+32,9);w64(p+40,24);       /* DT_RELAENT */
  w64(p+48,0x6ffffff9);w64(p+56,1);/* DT_RELACOUNT */
  w64(p+64,0);w64(p+72,0);        /* DT_NULL */
  w64(p+0x50,0x1080);
  w64(p+0x58,(uint64_t)(bad_type?1:8));
  w64(p+0x60,0x200);
}
static void make_elf(uint8_t *elf) {
  memset(elf,0,0x3000);
  elf[0]=0x7f;elf[1]='E';elf[2]='L';elf[3]='F';elf[4]=2;elf[5]=1;elf[6]=1;
  w16(elf+16,3);w16(elf+18,62);w32(elf+20,1);w64(elf+24,0);
  w64(elf+32,64);w16(elf+52,64);w16(elf+54,56);w16(elf+56,3);
  phdr(elf+64,1,5,0x1000,0,0x1000,0x1000);
  phdr(elf+120,1,6,0x2000,0x1000,0x1000,0x1000);
  phdr(elf+176,2,6,0x2000,0x1000,80,80);
  dynamic(elf+0x2000,0);
  elf[0x1000]=0x90;
}
static void cpio_header(uint8_t *p,const char *name,uint32_t mode,
                        uint32_t size,uint32_t ino) {
  uint32_t fields[13]={ino,mode,0,0,1,0,size,0,0,0,0,
                       (uint32_t)strlen(name)+1,0};
  memcpy(p,"070701",6);
  for(int f=0;f<13;f++)
    for(int n=0;n<8;n++) p[6+f*8+n]="0123456789abcdef"[(fields[f]>>(28-4*n))&15];
}
static size_t add_file(uint8_t *out,size_t at,const char *name,
                       uint32_t mode,const uint8_t *data,size_t size) {
  cpio_header(out+at,name,mode,(uint32_t)size,(uint32_t)(at+1));at+=110;
  size_t n=strlen(name)+1;memcpy(out+at,name,n);at+=n;at=(at+3)&~(size_t)3;
  if (size)
    memcpy(out + at, data, size);
  at += size;
  return (at + 3) & ~(size_t)3;
}
int main(void) {
  lamix_elf_allocator allocator={0,tracked_alloc,tracked_free};
  uint8_t elf[0x3000];make_elf(elf);lamix_elf_image image;
  assert(lamix_elf_prepare(elf,sizeof(elf),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_OK);
  assert(image.entry==LAMIX_ELF_LOAD_BASE&&image.segment_count==2&&image.size==0x2000);
  assert(image.bytes[0]==0x90);
  uint64_t relocated=0;memcpy(&relocated,image.bytes+0x1080,8);
  assert(relocated==LAMIX_ELF_LOAD_BASE+0x200);
  lamix_elf_release(&image);assert(allocations==0);
  make_elf(elf);dynamic(elf+0x2000,0);w64(elf+0x2060,0x2000);
  assert(lamix_elf_prepare(elf,sizeof(elf),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_OK);
  memcpy(&relocated,image.bytes+0x1080,8);
  assert(relocated==LAMIX_ELF_LOAD_BASE+image.size);
  lamix_elf_release(&image);assert(allocations==0);
  uint8_t bad[0x3000];make_elf(bad);bad[0]=0;
  assert(lamix_elf_prepare(bad,sizeof(bad),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_INVALID);
  make_elf(bad);w64(bad+64+16,UINT64_MAX-8);
  assert(lamix_elf_prepare(bad,sizeof(bad),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_INVALID);
  make_elf(bad);w32(bad+64+4,7);
  assert(lamix_elf_prepare(bad,sizeof(bad),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_INVALID);
  make_elf(bad);w64(bad+24,0x1000);
  assert(lamix_elf_prepare(bad,sizeof(bad),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_INVALID);
  make_elf(bad);w64(bad+120+16,0);
  assert(lamix_elf_prepare(bad,sizeof(bad),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_INVALID);
  make_elf(bad);w64(bad+120+40,0x2000);
  assert(lamix_elf_prepare(bad,sizeof(bad),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_OK);
  assert(image.size==0x3000&&image.bytes[0x2000]==0);
  lamix_elf_release(&image);
  make_elf(bad);dynamic(bad+0x2000,1);
  assert(lamix_elf_prepare(bad,sizeof(bad),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_UNSUPPORTED);
  make_elf(bad);phdr(bad+176,3,4,0x2000,0x1000,8,8);
  assert(lamix_elf_prepare(bad,sizeof(bad),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_UNSUPPORTED);
  make_elf(bad);fail_alloc=1;
  assert(lamix_elf_prepare(bad,sizeof(bad),LAMIX_ELF_LOAD_BASE,&allocator,&image)==LAMIX_ELF_NO_MEMORY);
  fail_alloc=0;assert(allocations==0);
  assert(lamix_elf_prepare(elf,sizeof(elf),LAMIX_ELF_LOAD_BASE-4096,
                           &allocator,&image)==LAMIX_ELF_INVALID);
  assert(lamix_elf_prepare(elf,sizeof(elf),LAMIX_ELF_USER_LIMIT,
                           &allocator,&image)==LAMIX_ELF_INVALID);

  uint8_t archive[512]={0}, payload[]={1,2,3,4};size_t used=0;
  used=add_file(archive,used,"alpha.elf",0100644,payload,sizeof(payload));
  used=add_file(archive,used,"TRAILER!!!",0,0,0);
  lamix_initramfs_file file;
  assert(lamix_initramfs_find(archive,used,"alpha.elf",&file)==LAMIX_INITRAMFS_OK);
  assert(file.size==sizeof(payload)&&!memcmp(file.data,payload,sizeof(payload)));
  assert(lamix_initramfs_find(archive,used,"missing",&file)==LAMIX_INITRAMFS_NOT_FOUND);
  assert(lamix_initramfs_find(archive,used-1,"alpha.elf",&file)==LAMIX_INITRAMFS_INVALID);
  archive[0]='x';assert(lamix_initramfs_find(archive,used,"alpha.elf",&file)==LAMIX_INITRAMFS_INVALID);
  memset(archive,0,sizeof(archive));used=0;
  used=add_file(archive,used,"Test01.elf",0100644,payload,sizeof(payload));
  used=add_file(archive,used,"alpha.elf",0100644,payload,sizeof(payload));
  used=add_file(archive,used,"TRAILER!!!",0,0,0);
  assert(lamix_initramfs_find(archive,used,"Test01.elf",&file)==LAMIX_INITRAMFS_OK);
  assert(file.size==sizeof(payload)&&!memcmp(file.data,payload,sizeof(payload)));
  assert(lamix_initramfs_find(archive,used,"alpha.elf",&file)==LAMIX_INITRAMFS_OK);
  memset(archive,0,sizeof(archive));used=0;
  used=add_file(archive,used,"alpha.elf",0100644,payload,sizeof(payload));
  used=add_file(archive,used,"alpha.elf",0100644,payload,sizeof(payload));
  used=add_file(archive,used,"TRAILER!!!",0,0,0);
  assert(lamix_initramfs_find(archive,used,"alpha.elf",&file)==LAMIX_INITRAMFS_INVALID);
  memset(archive,0,sizeof(archive));used=0;
  used=add_file(archive,used,"alpha.elf",0040755,payload,sizeof(payload));
  used=add_file(archive,used,"TRAILER!!!",0,0,0);
  assert(lamix_initramfs_find(archive,used,"alpha.elf",&file)==LAMIX_INITRAMFS_INVALID);
  return 0;
}
