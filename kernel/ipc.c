/* SPDX-License-Identifier: GPL-3.0-only */
#include "ipc.h"
typedef struct owned_endpoint owned_endpoint;
struct owned_endpoint {lamix_channel_endpoint ep;lamix_ipc_ops ops;};

static void clear(void *p,size_t n){uint8_t *b=p;for(size_t i=0;i<n;i++)b[i]=0;}
static lamix_ipc_status status_handle(lamix_handle_status s);

/* Queue attachments own channel endpoints, so queue edges must remain a DAG.
 * The graph lock also serializes queue mutation and protects the registry. */
static volatile uint8_t graph_gate;
static lamix_channel_core *graph_cores;
static uint64_t graph_epoch;
static void graph_lock(void){while(__atomic_test_and_set(&graph_gate,__ATOMIC_ACQUIRE)) {}}
static void graph_unlock(void){__atomic_clear(&graph_gate,__ATOMIC_RELEASE);}
static void graph_register(lamix_channel_core *c,const lamix_ipc_ops *ops){uintptr_t f=ops->lock?ops->lock(ops->context):0;graph_lock();c->registry_next=graph_cores;graph_cores=c;graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);}
static int graph_would_cycle(lamix_channel_core *source,lamix_object *const *objects,size_t count){
  if(++graph_epoch==0){for(lamix_channel_core *c=graph_cores;c;c=c->registry_next)c->graph_mark=0;graph_epoch=1;}
  uint64_t mark=graph_epoch;
  for(size_t i=0;i<count;i++)if(objects[i]->type==LAMIX_OBJECT_CHANNEL){owned_endpoint *e=(owned_endpoint *)objects[i]->context;e->ep.core->graph_mark=mark;}
  int changed=1;
  while(changed){changed=0;
    for(lamix_channel_core *c=graph_cores;c;c=c->registry_next)if(c->graph_mark==mark){
      for(unsigned side=0;side<2;side++)for(unsigned qi=0;qi<c->count[side];qi++){
        lamix_message *m=&c->queue[side][(c->head[side]+qi)%LAMIX_CHANNEL_DEPTH];
        for(size_t ai=0;ai<m->count;ai++)if(m->attachments[ai].object->type==LAMIX_OBJECT_CHANNEL){
          owned_endpoint *e=(owned_endpoint *)m->attachments[ai].object->context;lamix_channel_core *next=e->ep.core;
          if(next==source)return 1;
          if(next->graph_mark!=mark){next->graph_mark=mark;changed=1;}
        }
      }
    }
  }
  return source->graph_mark==mark;
}

static void core_release(lamix_ipc_ops *ops,lamix_channel_core *c){if(__atomic_sub_fetch(&c->references,1,__ATOMIC_ACQ_REL)==0){lamix_object *release_list[LAMIX_CHANNEL_DEPTH*2*LAMIX_CHANNEL_MAX_ATTACHMENTS];size_t n=0;uintptr_t f=ops->lock?ops->lock(ops->context):0;graph_lock();lamix_channel_core **at=&graph_cores;while(*at&&*at!=c)at=&(*at)->registry_next;if(*at)*at=c->registry_next;for(unsigned side=0;side<2;side++)while(c->count[side]){lamix_message *m=&c->queue[side][c->head[side]];for(size_t i=0;i<m->count;i++)release_list[n++]=m->attachments[i].object;clear(m,sizeof(*m));c->head[side]=(uint8_t)((c->head[side]+1)%LAMIX_CHANNEL_DEPTH);--c->count[side];}graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);for(size_t i=0;i<n;i++)lamix_object_release(release_list[i]);ops->deallocate(ops->context,c);}}
/* Core and endpoint allocation share the table allocator; endpoints keep the
 * callbacks in the core prefix through this private wrapper. */
typedef struct owned_core {lamix_channel_core core;lamix_ipc_ops ops;} owned_core;
typedef struct lamix_shm_object {lamix_object object;lamix_shm_ops ops;uint32_t pages;uint64_t frames[LAMIX_SHM_MAX_PAGES];} lamix_shm_object;
static void destroy_shm(lamix_object *o){lamix_shm_object *s=(lamix_shm_object *)o->context;for(uint32_t i=0;i<s->pages;i++)(void)s->ops.release_page(s->ops.memory.context,s->frames[i]);s->ops.memory.deallocate(s->ops.memory.context,s);}
static void destroy_endpoint(lamix_object *o){owned_endpoint *e=(owned_endpoint *)o->context;owned_core *c=(owned_core *)e->ep.core;lamix_ipc_ops ops=e->ops;uintptr_t f=ops.lock?ops.lock(ops.context):0;c->core.closed[e->ep.side]=1;if(ops.unlock)ops.unlock(ops.context,f);ops.deallocate(ops.context,e);core_release(&ops,&c->core);}
lamix_ipc_status lamix_channel_create(lamix_handle_table *t,const lamix_ipc_ops *ops,uint64_t *a,uint64_t *b){
 if(!t||!ops||!ops->allocate||!ops->deallocate||!a||!b)return LI_INVALID;
 owned_core *c=ops->allocate(ops->context,sizeof(*c));if(!c)return LI_NOMEM;clear(c,sizeof(*c));c->ops=*ops;c->core.references=2;
 owned_endpoint *x=ops->allocate(ops->context,sizeof(*x)),*y=ops->allocate(ops->context,sizeof(*y));if(!x||!y){if(x)ops->deallocate(ops->context,x);if(y)ops->deallocate(ops->context,y);ops->deallocate(ops->context,c);return LI_NOMEM;}
 *x=(owned_endpoint){.ep={.core=&c->core,.side=0},.ops=*ops};*y=(owned_endpoint){.ep={.core=&c->core,.side=1},.ops=*ops};lamix_object_init(&x->ep.object,LAMIX_OBJECT_CHANNEL,&x->ep,destroy_endpoint);lamix_object_init(&y->ep.object,LAMIX_OBJECT_CHANNEL,&y->ep,destroy_endpoint);
 lamix_handle_status s=lamix_handle_install(t,&x->ep.object,LAMIX_RIGHT_TRANSFER|LAMIX_RIGHT_CHANNEL_SEND|LAMIX_RIGHT_CHANNEL_RECEIVE,a);if(s!=LH_OK){ops->deallocate(ops->context,x);ops->deallocate(ops->context,y);ops->deallocate(ops->context,c);return status_handle(s);}
 s=lamix_handle_install(t,&y->ep.object,LAMIX_RIGHT_TRANSFER|LAMIX_RIGHT_CHANNEL_SEND|LAMIX_RIGHT_CHANNEL_RECEIVE,b);lamix_object_release(&x->ep.object);if(s!=LH_OK){lamix_handle_close(t,*a);lamix_object_release(&y->ep.object);return status_handle(s);}lamix_object_release(&y->ep.object);graph_register(&c->core,ops);return LI_OK;
}
static lamix_ipc_status status_handle(lamix_handle_status s){return s==LH_BADF?LI_BADF:s==LH_ACCESS?LI_ACCESS:s==LH_NOMEM?LI_NOMEM:s==LH_TOO_MANY?LI_TOO_MANY:LI_INVALID;}
lamix_ipc_status lamix_channel_send(lamix_handle_table *t,const lamix_ipc_ops *ops,uint64_t h,const void *data,size_t len,const lamix_attachment *att,size_t n){
 if(!t||!ops||(!data&&len)||(!att&&n)||len>LAMIX_CHANNEL_MAX_PAYLOAD||n>LAMIX_CHANNEL_MAX_ATTACHMENTS)return len>LAMIX_CHANNEL_MAX_PAYLOAD||n>LAMIX_CHANNEL_MAX_ATTACHMENTS?LI_MSGSIZE:LI_INVALID;
 uint64_t handles[LAMIX_CHANNEL_MAX_ATTACHMENTS]={0};uint32_t rights[LAMIX_CHANNEL_MAX_ATTACHMENTS]={0};uint8_t moves[LAMIX_CHANNEL_MAX_ATTACHMENTS]={0};lamix_object *expected[LAMIX_CHANNEL_MAX_ATTACHMENTS]={0},*transferred[LAMIX_CHANNEL_MAX_ATTACHMENTS]={0};
 for(size_t i=0;i<n;i++){if(!att[i].rights||(att[i].rights&~LAMIX_RIGHT_ALL)||(att[i].disposition!=LAMIX_ATTACHMENT_MOVE&&att[i].disposition!=LAMIX_ATTACHMENT_DUP)||att[i].reserved[0]||att[i].reserved[1]||att[i].reserved[2])return LI_INVALID;for(size_t j=0;j<i;j++)if(att[i].handle==att[j].handle)return LI_INVALID;handles[i]=att[i].handle;rights[i]=att[i].rights;moves[i]=(uint8_t)(att[i].disposition==LAMIX_ATTACHMENT_MOVE);}
 lamix_object *base=0;lamix_handle_status hs=lamix_handle_lookup(t,h,LAMIX_OBJECT_CHANNEL,LAMIX_RIGHT_CHANNEL_SEND,&base);if(hs!=LH_OK)return status_handle(hs);owned_endpoint *ep=(owned_endpoint *)base->context;lamix_channel_core *core=ep->ep.core;unsigned dst=1-ep->ep.side;
 lamix_message *m=ops->allocate(ops->context,sizeof(*m));if(!m){lamix_object_release(base);return LI_NOMEM;}*m=(lamix_message){.length=(uint16_t)len,.count=(uint16_t)n};if(len){const uint8_t *p=data;for(size_t i=0;i<len;i++)m->payload[i]=p[i];}
 for(size_t i=0;i<n;i++){uint32_t actual=0;hs=lamix_handle_lookup(t,att[i].handle,0,LAMIX_RIGHT_TRANSFER,&expected[i]);if(hs==LH_OK)hs=lamix_handle_rights(t,att[i].handle,&actual);if(hs!=LH_OK){for(size_t j=0;j<=i;j++)if(expected[j])lamix_object_release(expected[j]);ops->deallocate(ops->context,m);lamix_object_release(base);return status_handle(hs);}if(att[i].rights&~actual){for(size_t j=0;j<=i;j++)lamix_object_release(expected[j]);ops->deallocate(ops->context,m);lamix_object_release(base);return LI_ACCESS;}}
 uintptr_t f=ops->lock?ops->lock(ops->context):0;graph_lock();
 if(core->closed[dst]||core->closed[ep->ep.side]){graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);for(size_t i=0;i<n;i++)lamix_object_release(expected[i]);ops->deallocate(ops->context,m);lamix_object_release(base);return LI_CLOSED;}
 if(core->count[dst]>=LAMIX_CHANNEL_DEPTH){graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);for(size_t i=0;i<n;i++)lamix_object_release(expected[i]);ops->deallocate(ops->context,m);lamix_object_release(base);return LI_AGAIN;}
 if(graph_would_cycle(core,expected,n)){graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);for(size_t i=0;i<n;i++)lamix_object_release(expected[i]);ops->deallocate(ops->context,m);lamix_object_release(base);return LI_INVALID;}
 hs=lamix_handle_transfer_batch(t,handles,rights,moves,expected,n,transferred);
 if(hs!=LH_OK){graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);for(size_t i=0;i<n;i++)lamix_object_release(expected[i]);ops->deallocate(ops->context,m);lamix_object_release(base);return status_handle(hs);}
 for(size_t i=0;i<n;i++){lamix_object_release(expected[i]);m->attachments[i].object=transferred[i];m->attachments[i].rights=rights[i];}
 unsigned at=(core->head[dst]+core->count[dst])%LAMIX_CHANNEL_DEPTH;core->queue[dst][at]=*m;++core->count[dst];ops->deallocate(ops->context,m);graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);lamix_object_release(base);return LI_OK;
}
lamix_ipc_status lamix_channel_receive(lamix_handle_table *t,const lamix_ipc_ops *ops,uint64_t h,void *out,size_t cap,size_t *len,lamix_attachment *att,size_t ncap,size_t *count){
 if(!t||!ops||!len||!count||(!out&&cap)||(!att&&ncap))return LI_INVALID;
 lamix_object *base=0;lamix_handle_status hs=lamix_handle_lookup(t,h,LAMIX_OBJECT_CHANNEL,LAMIX_RIGHT_CHANNEL_RECEIVE,&base);if(hs!=LH_OK)return status_handle(hs);
 owned_endpoint *ep=(owned_endpoint *)base->context;lamix_channel_core *core=ep->ep.core;unsigned side=ep->ep.side;uintptr_t f=ops->lock?ops->lock(ops->context):0;graph_lock();
 if(!core->count[side]){lamix_ipc_status r=core->closed[1-side]?LI_CLOSED:LI_AGAIN;graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);lamix_object_release(base);return r;}
 lamix_message *m=&core->queue[side][core->head[side]];if(cap<m->length||ncap<m->count){graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);lamix_object_release(base);return LI_MSGSIZE;}
 uint64_t installed[LAMIX_CHANNEL_MAX_ATTACHMENTS]={0};size_t done=0;lamix_handle_status install_status=LH_OK;
 for(;done<m->count;done++){install_status=lamix_handle_install(t,m->attachments[done].object,m->attachments[done].rights,&installed[done]);if(install_status!=LH_OK)break;}
 if(done<m->count){while(done)(void)lamix_handle_close(t,installed[--done]);graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);lamix_object_release(base);return install_status==LH_TOO_MANY?LI_TOO_MANY:install_status==LH_NOMEM?LI_NOMEM:LI_INVALID;}
 /* Commit copyout and dequeue only after every capability slot is reserved. */
 if(m->length&&out){for(size_t i=0;i<m->length;i++)((uint8_t *)out)[i]=m->payload[i];}*len=m->length;*count=m->count;
 for(size_t i=0;i<m->count;i++)att[i]=(lamix_attachment){.handle=installed[i],.rights=m->attachments[i].rights,.disposition=LAMIX_ATTACHMENT_DUP};
 lamix_object *released[LAMIX_CHANNEL_MAX_ATTACHMENTS];size_t released_count=m->count;for(size_t i=0;i<released_count;i++)released[i]=m->attachments[i].object;
 clear(m,sizeof(*m));core->head[side]=(uint8_t)((core->head[side]+1)%LAMIX_CHANNEL_DEPTH);--core->count[side];graph_unlock();if(ops->unlock)ops->unlock(ops->context,f);
 for(size_t i=0;i<released_count;i++)lamix_object_release(released[i]);
 lamix_object_release(base);return LI_OK;
}
void lamix_ipc_close_all(lamix_handle_table *t){(void)t;}
lamix_ipc_status lamix_shm_create(lamix_handle_table *t,const lamix_shm_ops *ops,uint32_t pages,uint64_t *out){
 if(!t||!ops||!out||!ops->allocate_page||!ops->release_page||!ops->access_page||pages<1||pages>LAMIX_SHM_MAX_PAGES)return LI_INVALID;
 lamix_shm_object *s=ops->memory.allocate(ops->memory.context,sizeof(*s));if(!s)return LI_NOMEM;clear(s,sizeof(*s));s->ops=*ops;s->pages=pages;lamix_object_init(&s->object,LAMIX_OBJECT_SHM,s,destroy_shm);
 for(uint32_t i=0;i<pages;i++){if(!ops->allocate_page(ops->memory.context,&s->frames[i])){s->pages=i;lamix_object_release(&s->object);return LI_NOMEM;}volatile uint8_t *p=ops->access_page(ops->memory.context,s->frames[i]);if(!p){s->pages=i+1;lamix_object_release(&s->object);return LI_NOMEM;}for(size_t j=0;j<4096;j++)p[j]=0;}
 lamix_handle_status r=lamix_handle_install(t,&s->object,LAMIX_RIGHT_TRANSFER|LAMIX_RIGHT_SHM_MAP|LAMIX_RIGHT_SHM_READ|LAMIX_RIGHT_SHM_WRITE,out);lamix_object_release(&s->object);return r==LH_OK?LI_OK:r==LH_TOO_MANY?LI_TOO_MANY:LI_NOMEM;
}
lamix_ipc_status lamix_shm_map(lamix_handle_table *t,const lamix_shm_ops *ops,lamix_shm_mapping **head,uint64_t handle,uint64_t va,uint32_t rights){
 if(!t||!ops||!head||!ops->map_page||!ops->unmap_page||(va&4095)||va>=UINT64_C(0x0000800000000000)||(rights&~(LAMIX_RIGHT_SHM_READ|LAMIX_RIGHT_SHM_WRITE))||!rights)return LI_INVALID;
 lamix_object *o=0;lamix_handle_status hs=lamix_handle_lookup(t,handle,LAMIX_OBJECT_SHM,LAMIX_RIGHT_SHM_MAP,&o);if(hs!=LH_OK)return status_handle(hs);
 uint32_t available=0;(void)lamix_handle_rights(t,handle,&available);if(rights&~available){lamix_object_release(o);return LI_ACCESS;}
 lamix_shm_object *s=(lamix_shm_object *)o->context;uint64_t length=(uint64_t)s->pages*4096;const uint64_t user_base=UINT64_C(0x0000008000000000),user_limit=UINT64_C(0x0000010000000000);if(va<user_base||va>=user_limit||length>user_limit-va){lamix_object_release(o);return LI_INVALID;}
 for(lamix_shm_mapping *m=*head;m;m=m->next)if(va<m->address+(uint64_t)m->pages*4096&&m->address<va+length){lamix_object_release(o);return LI_EXISTS;}
 lamix_shm_mapping *record=ops->memory.allocate(ops->memory.context,sizeof(*record));if(!record){lamix_object_release(o);return LI_NOMEM;}
 uint32_t permissions=rights;uint32_t mapped=0;for(;mapped<s->pages;mapped++)if(!ops->map_page(ops->memory.context,va+(uint64_t)mapped*4096,s->frames[mapped],permissions))break;
 if(mapped!=s->pages){while(mapped)(void)ops->unmap_page(ops->memory.context,va+(uint64_t)--mapped*4096);ops->memory.deallocate(ops->memory.context,record);lamix_object_release(o);return LI_EXISTS;}
 *record=(lamix_shm_mapping){.next=*head,.object=o,.address=va,.pages=s->pages};*head=record;return LI_OK;
}
lamix_ipc_status lamix_shm_unmap(const lamix_shm_ops *ops,lamix_shm_mapping **head,uint64_t va){
 if(!ops||!head)return LI_INVALID;
 lamix_shm_mapping **at=head;while(*at&&(*at)->address!=va)at=&(*at)->next;
 if(!*at)return LI_INVALID;
 lamix_shm_mapping *m=*at;
 for(uint32_t i=0;i<m->pages;i++)if(!ops->unmap_page(ops->memory.context,m->address+(uint64_t)i*4096))return LI_INVALID;
 *at=m->next;lamix_object_release(m->object);ops->memory.deallocate(ops->memory.context,m);return LI_OK;
}
