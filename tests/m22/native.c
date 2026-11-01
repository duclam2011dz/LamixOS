/* SPDX-License-Identifier: GPL-3.0-only */
#include "handle.h"
#include "ipc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static long tracked_allocations;
static void *alloc(void *c,size_t n){(void)c;void *p=calloc(1,n);if(p)++tracked_allocations;return p;}
static void dealloc(void *c,void *p){(void)c;if(p)--tracked_allocations;free(p);}
static void destroy(lamix_object *o){free(o->context);free(o);}
static lamix_object *object(void){lamix_object *o=calloc(1,sizeof(*o));int *v=calloc(1,sizeof(*v));assert(o&&v);lamix_object_init(o,LAMIX_OBJECT_SHM,v,destroy);return o;}
typedef struct page_fixture {uint8_t pages[16][4096];uint8_t allocated[16],mapped[16];} page_fixture;
static int page_alloc(void *c,uint64_t *id){page_fixture *f=c;for(unsigned i=0;i<16;i++)if(!f->allocated[i]){f->allocated[i]=1;*id=i;return 1;}return 0;}
static int page_free(void *c,uint64_t id){page_fixture *f=c;if(id>=16||!f->allocated[id])return 0;f->allocated[id]=0;return 1;}
static volatile uint8_t *page_access(void *c,uint64_t id){page_fixture *f=c;return id<16&&f->allocated[id]?f->pages[id]:0;}
static int map_page(void *c,uint64_t va,uint64_t id,uint32_t rights){page_fixture *f=c;(void)rights;unsigned slot=(unsigned)((va>>12)&15);if(slot>=16||f->mapped[slot]||id>=16||!f->allocated[id])return 0;f->mapped[slot]=1;return 1;}
static int unmap_page(void *c,uint64_t va){page_fixture *f=c;unsigned slot=(unsigned)((va>>12)&15);if(slot>=16||!f->mapped[slot])return 0;f->mapped[slot]=0;return 1;}
int main(void){
 lamix_handle_table t;assert(lamix_handle_table_init(&t,0,alloc,dealloc,0,0)==LH_OK);
 lamix_object *o=object();uint64_t h=0,dup=0;
 assert(lamix_handle_install(&t,o,LAMIX_RIGHT_TRANSFER|LAMIX_RIGHT_SHM_READ,&h)==LH_OK);
 lamix_object_release(o);
 assert(lamix_handle_duplicate(&t,h,LAMIX_RIGHT_SHM_READ,&dup)==LH_OK);
 assert(lamix_handle_duplicate(&t,h,LAMIX_RIGHT_SHM_WRITE,&dup)==LH_ACCESS);
 assert(lamix_handle_duplicate(&t,h,UINT32_C(1)<<31,&dup)==LH_INVALID);
 assert(lamix_handle_close(&t,h)==LH_OK);
 assert(lamix_handle_lookup(&t,h,0,0,0)==LH_BADF);
 o=object();uint64_t replacement=0;assert(lamix_handle_install(&t,o,LAMIX_RIGHT_SHM_READ,&replacement)==LH_OK);lamix_object_release(o);
 assert((uint32_t)replacement==(uint32_t)h&&(uint32_t)(replacement>>32)!=(uint32_t)(h>>32));
 assert(lamix_handle_lookup(&t,h,0,0,0)==LH_BADF);
 for(unsigned i=0;i<254;i++){o=object();uint64_t next;assert(lamix_handle_install(&t,o,LAMIX_RIGHT_SHM_READ,&next)==LH_OK);lamix_object_release(o);}
 assert(t.capacity==LAMIX_HANDLE_MAX);
 o=object();uint64_t overflow;assert(lamix_handle_install(&t,o,LAMIX_RIGHT_SHM_READ,&overflow)==LH_TOO_MANY);lamix_object_release(o);
 lamix_handle_table_destroy(&t);

 lamix_handle_table a,b;assert(lamix_handle_table_init(&a,0,alloc,dealloc,0,0)==LH_OK);assert(lamix_handle_table_init(&b,0,alloc,dealloc,0,0)==LH_OK);
 lamix_ipc_ops ops={.allocate=alloc,.deallocate=dealloc};uint64_t x,y;
 assert(lamix_channel_create(&a,&ops,&x,&y)==LI_OK);
 const char message[]="bounded ipc";assert(lamix_channel_send(&a,&ops,x,message,sizeof(message),0,0)==LI_OK);
 char received[32]={0};size_t length=0,count=0;assert(lamix_channel_receive(&a,&ops,y,received,sizeof(received),&length,0,0,&count)==LI_OK);
 assert(length==sizeof(message)&&!memcmp(received,message,length)&&count==0);
 for(unsigned i=0;i<LAMIX_CHANNEL_DEPTH;i++)assert(lamix_channel_send(&a,&ops,x,message,sizeof(message),0,0)==LI_OK);
 assert(lamix_channel_send(&a,&ops,x,message,sizeof(message),0,0)==LI_AGAIN);
 assert(lamix_channel_receive(&a,&ops,y,received,1,&length,0,0,&count)==LI_MSGSIZE);
 assert(lamix_channel_receive(&a,&ops,y,received,sizeof(received),&length,0,0,&count)==LI_OK&&length==sizeof(message)&&!memcmp(received,message,length));
 lamix_handle_table_destroy(&a);lamix_handle_table_destroy(&b);
 lamix_handle_table tx;assert(lamix_handle_table_init(&tx,0,alloc,dealloc,0,0)==LH_OK);
 uint64_t txa,txb,source;assert(lamix_channel_create(&tx,&ops,&txa,&txb)==LI_OK);
 o=object();assert(lamix_handle_install(&tx,o,LAMIX_RIGHT_TRANSFER|LAMIX_RIGHT_SHM_READ,&source)==LH_OK);lamix_object_release(o);
 lamix_attachment move={.handle=source,.rights=LAMIX_RIGHT_SHM_READ,.disposition=LAMIX_ATTACHMENT_MOVE};
 assert(lamix_channel_send(&tx,&ops,txa,message,sizeof(message),&move,1)==LI_OK);
 assert(lamix_handle_lookup(&tx,source,0,0,0)==LH_BADF);
 lamix_attachment received_cap[1];assert(lamix_channel_receive(&tx,&ops,txb,received,sizeof(received),&length,received_cap,0,&count)==LI_MSGSIZE);
 assert(lamix_channel_receive(&tx,&ops,txb,received,sizeof(received),&length,received_cap,1,&count)==LI_OK&&count==1);
 uint32_t got_rights=0;assert(lamix_handle_rights(&tx,received_cap[0].handle,&got_rights)==LH_OK&&got_rights==LAMIX_RIGHT_SHM_READ);
 o=object();assert(lamix_handle_install(&tx,o,LAMIX_RIGHT_TRANSFER|LAMIX_RIGHT_SHM_READ,&source)==LH_OK);lamix_object_release(o);
 lamix_attachment duplicate={.handle=source,.rights=LAMIX_RIGHT_SHM_READ,.disposition=LAMIX_ATTACHMENT_DUP};
 assert(lamix_channel_send(&tx,&ops,txa,message,sizeof(message),&duplicate,1)==LI_OK);
 assert(lamix_handle_lookup(&tx,source,LAMIX_OBJECT_SHM,LAMIX_RIGHT_SHM_READ,0)==LH_OK);
 lamix_attachment repeated[2]={duplicate,duplicate};assert(lamix_channel_send(&tx,&ops,txa,message,sizeof(message),repeated,2)==LI_INVALID);
 assert(lamix_channel_receive(&tx,&ops,txb,received,sizeof(received),&length,received_cap,1,&count)==LI_OK&&count==1);
 assert(lamix_handle_close(&tx,source)==LH_OK);assert(lamix_handle_close(&tx,received_cap[0].handle)==LH_OK);lamix_handle_table_destroy(&tx);
 lamix_handle_table drain;assert(lamix_handle_table_init(&drain,0,alloc,dealloc,0,0)==LH_OK);uint64_t send_end,recv_end;assert(lamix_channel_create(&drain,&ops,&send_end,&recv_end)==LI_OK);
 assert(lamix_channel_send(&drain,&ops,send_end,message,sizeof(message),0,0)==LI_OK);assert(lamix_handle_close(&drain,send_end)==LH_OK);
 assert(lamix_channel_receive(&drain,&ops,recv_end,received,sizeof(received),&length,0,0,&count)==LI_OK&&length==sizeof(message));
 assert(lamix_channel_receive(&drain,&ops,recv_end,received,sizeof(received),&length,0,0,&count)==LI_CLOSED);assert(lamix_handle_close(&drain,recv_end)==LH_OK);lamix_handle_table_destroy(&drain);
 lamix_handle_table rollback;assert(lamix_handle_table_init(&rollback,0,alloc,dealloc,0,0)==LH_OK);assert(lamix_channel_create(&rollback,&ops,&send_end,&recv_end)==LI_OK);
 o=object();assert(lamix_handle_install(&rollback,o,LAMIX_RIGHT_TRANSFER|LAMIX_RIGHT_SHM_READ,&source)==LH_OK);lamix_object_release(o);
 for(unsigned i=0;i<LAMIX_CHANNEL_DEPTH;i++)assert(lamix_channel_send(&rollback,&ops,send_end,message,sizeof(message),0,0)==LI_OK);
 move.handle=source;assert(lamix_channel_send(&rollback,&ops,send_end,message,sizeof(message),&move,1)==LI_AGAIN);assert(lamix_handle_lookup(&rollback,source,0,LAMIX_RIGHT_SHM_READ,0)==LH_OK);
 lamix_handle_table_destroy(&rollback);
 /* A channel endpoint cannot be queued into its own core. */
 long before_cycle=tracked_allocations;lamix_handle_table self;assert(lamix_handle_table_init(&self,0,alloc,dealloc,0,0)==LH_OK);uint64_t self_a,self_b;assert(lamix_channel_create(&self,&ops,&self_a,&self_b)==LI_OK);lamix_attachment self_move={.handle=self_a,.rights=LAMIX_RIGHT_CHANNEL_SEND|LAMIX_RIGHT_CHANNEL_RECEIVE,.disposition=LAMIX_ATTACHMENT_MOVE};assert(lamix_channel_send(&self,&ops,self_a,message,sizeof(message),&self_move,1)==LI_INVALID);assert(lamix_handle_lookup(&self,self_a,LAMIX_OBJECT_CHANNEL,LAMIX_RIGHT_CHANNEL_SEND,0)==LH_OK);lamix_handle_table_destroy(&self);assert(tracked_allocations==before_cycle);
 /* A->B followed by B->A is rejected, so queued capabilities stay acyclic. */
 before_cycle=tracked_allocations;lamix_handle_table graph;assert(lamix_handle_table_init(&graph,0,alloc,dealloc,0,0)==LH_OK);uint64_t a0,a1,b0,b1;assert(lamix_channel_create(&graph,&ops,&a0,&a1)==LI_OK);assert(lamix_channel_create(&graph,&ops,&b0,&b1)==LI_OK);lamix_attachment edge={.handle=b1,.rights=LAMIX_RIGHT_CHANNEL_SEND|LAMIX_RIGHT_CHANNEL_RECEIVE,.disposition=LAMIX_ATTACHMENT_MOVE};assert(lamix_channel_send(&graph,&ops,a0,message,sizeof(message),&edge,1)==LI_OK);assert(lamix_handle_lookup(&graph,b1,0,0,0)==LH_BADF);edge.handle=a1;assert(lamix_channel_send(&graph,&ops,b0,message,sizeof(message),&edge,1)==LI_INVALID);assert(lamix_handle_lookup(&graph,a1,LAMIX_OBJECT_CHANNEL,LAMIX_RIGHT_CHANNEL_SEND,0)==LH_OK);lamix_handle_table_destroy(&graph);assert(tracked_allocations==before_cycle);
 page_fixture pages={0};lamix_handle_table sh;assert(lamix_handle_table_init(&sh,0,alloc,dealloc,0,0)==LH_OK);
 lamix_shm_ops so={.memory={.context=0,.allocate=alloc,.deallocate=dealloc},.allocate_page=page_alloc,.release_page=page_free,.access_page=page_access,.map_page=map_page,.unmap_page=unmap_page};
 /* Page callbacks share the backing fixture while metadata uses host allocation. */
 so.memory.context=&pages;uint64_t shm;assert(lamix_shm_create(&sh,&so,2,&shm)==LI_OK);
 lamix_shm_mapping *mappings=0;assert(lamix_shm_map(&sh,&so,&mappings,shm,0x1000,LAMIX_RIGHT_SHM_READ)==LI_INVALID);
 assert(lamix_shm_map(&sh,&so,&mappings,shm,UINT64_C(0x0000008000001000),LAMIX_RIGHT_SHM_READ|LAMIX_RIGHT_SHM_WRITE)==LI_OK);
 assert(mappings&&mappings->pages==2);assert(lamix_handle_close(&sh,shm)==LH_OK);
 assert(pages.allocated[0]&&pages.allocated[1]);assert(lamix_shm_unmap(&so,&mappings,UINT64_C(0x0000008000001000))==LI_OK);
 assert(!mappings&&!pages.allocated[0]&&!pages.allocated[1]);lamix_handle_table_destroy(&sh);return 0;
}
