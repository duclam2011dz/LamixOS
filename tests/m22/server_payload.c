/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/syscall.h"
#define USER_DATA UINT64_C(0x0000008000100000)
static const char request[]="shared capability";
static const char response[]="reply from process B";
static const char ready[]="M22:SERVER:READY\n",received_marker[]="M22:SERVER:RECEIVED\n";
__attribute__((noreturn,section(".text.start"))) void _start(void){
  volatile uint64_t *seed=(volatile uint64_t *)(uintptr_t)USER_DATA;
  uint64_t endpoint=seed[0],length=0,count=2;char buffer[64]={0};lamix_attachment attachments[2];
  (void)lamix_debug_write(ready,sizeof(ready)-1);
  for(unsigned tries=0;;tries++){int64_t r=lamix_channel_receive(endpoint,buffer,sizeof(buffer),&length,attachments,&count);if(r==0)break;if(r!=-LAMIX_EAGAIN)lamix_exit(1);if(tries==1000)lamix_exit(4);(void)lamix_yield();}
  (void)lamix_debug_write(received_marker,sizeof(received_marker)-1);
  if(length!=sizeof(request)||buffer[0]!='s'||count!=2||attachments[0].rights!=(LAMIX_RIGHT_SHM_MAP|LAMIX_RIGHT_SHM_READ|LAMIX_RIGHT_SHM_WRITE)||attachments[1].rights!=(LAMIX_RIGHT_SHM_MAP|LAMIX_RIGHT_SHM_READ))lamix_exit(2);
  if(lamix_shm_map(attachments[0].handle,UINT64_C(0x0000008000600000),LAMIX_RIGHT_SHM_READ|LAMIX_RIGHT_SHM_WRITE)!=0)lamix_exit(5);
  if(lamix_shm_map(attachments[1].handle,UINT64_C(0x0000008000610000),LAMIX_RIGHT_SHM_READ)!=0)lamix_exit(7);
  volatile uint64_t *shared=(volatile uint64_t *)(uintptr_t)UINT64_C(0x0000008000600000);
  if(*shared!=UINT64_C(0x1122334455667788))lamix_exit(6);
  *shared=UINT64_C(0x8877665544332211);
  if(lamix_channel_send(endpoint,response,sizeof(response),0,0)!=0)lamix_exit(3);
  /* The following user write must fault because the second mapping is read-only. */
  *(volatile uint64_t *)(uintptr_t)UINT64_C(0x0000008000610000)=1;
  (void)lamix_handle_close(endpoint);lamix_exit(0);for(;;)__asm__ volatile("pause");
}
