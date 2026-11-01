/* SPDX-License-Identifier: GPL-3.0-only */
#include "lamix/syscall.h"
static const char marker[]="M22:USER:ABI:PASS\n";
static const char message[]="channel-roundtrip";
static const char shared_request[]="shared capability";
static const char stage_abi[]="M22:STAGE:ABI\n",stage_local[]="M22:STAGE:LOCAL\n",stage_shm[]="M22:STAGE:SHM\n",stage_send[]="M22:STAGE:SEND\n",stage_receive[]="M22:STAGE:RECEIVE\n";
static void trace_result(const char *label,int64_t value){static const char digits[]="0123456789abcdef";char text[48];unsigned i=0;while(label[i]&&i<20){text[i]=label[i];++i;}for(int shift=60;shift>=0;shift-=4)text[i++]=digits[((uint64_t)value>>shift)&15];text[i++]='\n';(void)lamix_debug_write(text,i);}
#define SHM_ADDRESS UINT64_C(0x0000008000600000)
#define USER_DATA UINT64_C(0x0000008000100000)
__attribute__((noreturn,section(".text.start"))) void _start(void){
  uint64_t a=0,b=0,copy=0,length=0,count=0;char received[32]={0};
  volatile uint64_t *seed=(volatile uint64_t *)(uintptr_t)USER_DATA;
  trace_result("M22:USER_SEED=",(int64_t)seed[0]);
  if(lamix_version()!=INT64_C(0x0000000100000001))lamix_exit(1);
  if(lamix_syscall6(99,0,0,0,0,0,0)!=-LAMIX_ENOSYS)lamix_exit(20);
  (void)lamix_debug_write(stage_abi,sizeof(stage_abi)-1);
  if(lamix_channel_create(&a,&b)!=0||!a||!b)lamix_exit(2);
  if(lamix_channel_send(a,(const void *)(uintptr_t)UINT64_MAX,1,0,0)!=-LAMIX_EFAULT)lamix_exit(21);
  if(lamix_handle_duplicate(a,LAMIX_RIGHT_CHANNEL_SEND,(uint64_t *)(uintptr_t)UINT64_MAX)!=-LAMIX_EFAULT)lamix_exit(24);
  if(lamix_handle_duplicate(a,UINT32_C(1)<<31,&copy)!=-LAMIX_EINVAL)lamix_exit(22);
  if(lamix_handle_duplicate(a,LAMIX_RIGHT_CHANNEL_RECEIVE,&copy)!=0||
     lamix_channel_send(copy,message,sizeof(message),0,0)!=-LAMIX_EACCES||
     lamix_handle_close(copy)!=0)lamix_exit(23);
  if(lamix_channel_send(a,message,sizeof(message),0,0)!=0)lamix_exit(3);
  if(lamix_channel_receive(b,(void *)(uintptr_t)UINT64_MAX,sizeof(received),&length,0,&count)!=-LAMIX_EFAULT)lamix_exit(25);
  if(lamix_channel_receive(b,received,sizeof(received),&length,0,&count)!=0||
     length!=sizeof(message)||count||received[0]!='c')lamix_exit(4);
  if(lamix_handle_duplicate(a,LAMIX_RIGHT_CHANNEL_SEND,&copy)!=0||!copy)lamix_exit(5);
  if(lamix_channel_send(copy,message,sizeof(message),0,0)!=0)lamix_exit(6);
  if(lamix_channel_receive(b,received,sizeof(received),&length,0,&count)!=0)lamix_exit(7);
  if(lamix_handle_close(copy)!=0||lamix_handle_close(a)!=0)lamix_exit(8);
  if(lamix_channel_send(b,message,sizeof(message),0,0)!=-LAMIX_ECLOSED)lamix_exit(9);
  (void)lamix_debug_write(stage_local,sizeof(stage_local)-1);
  uint64_t shm=0;if(lamix_shm_create(1,&shm)!=0||!shm)lamix_exit(10);
  if(lamix_shm_map(shm,SHM_ADDRESS,LAMIX_RIGHT_SHM_READ|LAMIX_RIGHT_SHM_WRITE)!=0)lamix_exit(11);
  volatile uint64_t *shared=(volatile uint64_t *)(uintptr_t)SHM_ADDRESS;
  if(*shared!=0)lamix_exit(12);
  *shared=UINT64_C(0x4d323253484d);
  if(*shared!=UINT64_C(0x4d323253484d))lamix_exit(13);
  if(lamix_handle_close(shm)!=0||*shared!=UINT64_C(0x4d323253484d))lamix_exit(14);
  if(lamix_shm_unmap(SHM_ADDRESS)!=0||lamix_shm_map(shm,SHM_ADDRESS,LAMIX_RIGHT_SHM_READ)!=-6)lamix_exit(15);
  (void)lamix_debug_write(stage_shm,sizeof(stage_shm)-1);
  uint64_t transferred=0;
  if(lamix_shm_create(1,&transferred)!=0||lamix_shm_map(transferred,SHM_ADDRESS,LAMIX_RIGHT_SHM_READ|LAMIX_RIGHT_SHM_WRITE)!=0)lamix_exit(16);
  volatile uint64_t *shared_transfer=(volatile uint64_t *)(uintptr_t)SHM_ADDRESS;
  *shared_transfer=UINT64_C(0x1122334455667788);
  uint64_t readonly=0;if(lamix_shm_create(1,&readonly)!=0)lamix_exit(16);
  lamix_attachment attachments[2]={
    {.handle=transferred,.rights=LAMIX_RIGHT_SHM_MAP|LAMIX_RIGHT_SHM_READ|LAMIX_RIGHT_SHM_WRITE,.disposition=LAMIX_ATTACHMENT_MOVE},
    {.handle=readonly,.rights=LAMIX_RIGHT_SHM_MAP|LAMIX_RIGHT_SHM_READ,.disposition=LAMIX_ATTACHMENT_MOVE}
  };
  int64_t remote_send=lamix_channel_send(seed[0],shared_request,sizeof(shared_request),attachments,2);
  trace_result("M22:REMOTE_SEND=",remote_send);
  if(remote_send!=0||lamix_handle_close(transferred)!=-LAMIX_EBADF||lamix_handle_close(readonly)!=-LAMIX_EBADF)lamix_exit(16);
  (void)lamix_debug_write(stage_send,sizeof(stage_send)-1);
  for(;;){int64_t r=lamix_channel_receive(seed[0],received,sizeof(received),&length,0,&count);if(r==0)break;if(r!=-LAMIX_EAGAIN)lamix_exit(17);(void)lamix_yield();}
  if(length!=sizeof("reply from process B")||received[0]!='r'||*shared_transfer!=UINT64_C(0x8877665544332211))lamix_exit(18);
  if(lamix_shm_unmap(SHM_ADDRESS)!=0)lamix_exit(19);
  (void)lamix_debug_write(stage_receive,sizeof(stage_receive)-1);
  (void)lamix_handle_close(seed[0]);
  (void)lamix_debug_write(marker,sizeof(marker)-1);
  (void)lamix_handle_close(b);
  lamix_exit(0);
  for(;;)__asm__ volatile("pause");
}
