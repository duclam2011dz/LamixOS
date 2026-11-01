/* SPDX-License-Identifier: GPL-3.0-only */
#include "handle.h"

void lamix_object_init(lamix_object *o, lamix_object_type type, void *context,
                       void (*destroy)(lamix_object *)) {
  *o = (lamix_object){.references=1,.type=type,.context=context,.destroy=destroy};
}
void lamix_object_retain(lamix_object *o) { if (o) __atomic_add_fetch(&o->references,1,__ATOMIC_RELAXED); }
void lamix_object_release(lamix_object *o) {
  if (o && __atomic_sub_fetch(&o->references,1,__ATOMIC_ACQ_REL)==0 && o->destroy) o->destroy(o);
}
static uintptr_t enter(lamix_handle_table *t) { return t->lock ? t->lock(t->context) : 0; }
static void leave(lamix_handle_table *t, uintptr_t f) { if (t->unlock) t->unlock(t->context,f); }
static int grow(lamix_handle_table *t) {
  if (t->capacity>=LAMIX_HANDLE_MAX) return 0;
  size_t n=t->capacity*2; if(n>LAMIX_HANDLE_MAX)n=LAMIX_HANDLE_MAX;
  lamix_handle_entry *p=t->allocate(t->context,n*sizeof(*p)); if(!p)return -1;
  for(size_t i=0;i<n;i++) p[i]=(lamix_handle_entry){0};
  for(size_t i=0;i<t->capacity;i++)p[i]=t->entries[i];
  lamix_handle_entry *old=t->entries;t->entries=p;t->capacity=n;t->deallocate(t->context,old);return 1;
}
lamix_handle_status lamix_handle_table_init(lamix_handle_table *t, void *ctx,
  void *(*a)(void *,size_t),void (*d)(void *,void *),uintptr_t (*l)(void *),void (*u)(void *,uintptr_t)) {
  if(!t||!a||!d)return LH_INVALID;
  *t=(lamix_handle_table){.context=ctx,.allocate=a,.deallocate=d,.lock=l,.unlock=u};
  t->entries=a(ctx,LAMIX_HANDLE_INITIAL*sizeof(*t->entries));if(!t->entries)return LH_NOMEM;
  t->capacity=LAMIX_HANDLE_INITIAL;for(size_t i=0;i<t->capacity;i++)t->entries[i]=(lamix_handle_entry){.generation=1};return LH_OK;
}
void lamix_handle_table_destroy(lamix_handle_table *t) {
  if(!t||!t->entries)return;
  for(size_t i=0;i<t->capacity;i++)if(t->entries[i].object)lamix_object_release(t->entries[i].object);
  t->deallocate(t->context,t->entries);*t=(lamix_handle_table){0};
}
lamix_handle_status lamix_handle_install(lamix_handle_table *t,lamix_object *o,uint32_t rights,uint64_t *out) {
  if(!t||!t->entries||!o||!out||!rights||(rights&~LAMIX_RIGHT_ALL))return LH_INVALID;
  uintptr_t f=enter(t);
  for(;;){for(size_t i=0;i<t->capacity;i++){lamix_handle_entry *e=&t->entries[i];if(!e->object&&!e->retired){if(!e->generation)e->generation=1;e->object=o;e->rights=rights;lamix_object_retain(o);*out=((uint64_t)e->generation<<32)|(uint32_t)i;leave(t,f);return LH_OK;}}
    int g=grow(t);if(g<0){leave(t,f);return LH_NOMEM;}if(!g){leave(t,f);return LH_TOO_MANY;}}
}
static lamix_handle_entry *find(lamix_handle_table *t,uint64_t h) {
  uint32_t slot=(uint32_t)h,gen=(uint32_t)(h>>32);if(!gen||slot>=t->capacity)return 0;
  lamix_handle_entry *e=&t->entries[slot];return e->object&&e->generation==gen?e:0;
}
lamix_handle_status lamix_handle_lookup(lamix_handle_table *t,uint64_t h,lamix_object_type type,uint32_t rights,lamix_object **out) {
  if(!t||!t->entries)return LH_INVALID;
  uintptr_t f=enter(t);lamix_handle_entry *e=find(t,h);
  if(!e){leave(t,f);return LH_BADF;}if((type&&e->object->type!=type)||(rights&~e->rights)){leave(t,f);return LH_ACCESS;}
  if(out){*out=e->object;lamix_object_retain(*out);}leave(t,f);return LH_OK;
}
lamix_handle_status lamix_handle_rights(lamix_handle_table *t,uint64_t h,uint32_t *rights) {
  if(!t||!t->entries||!rights)return LH_INVALID;
  uintptr_t f=enter(t);lamix_handle_entry *e=find(t,h);
  if(!e){leave(t,f);return LH_BADF;}*rights=e->rights;leave(t,f);return LH_OK;
}
lamix_handle_status lamix_handle_remove(lamix_handle_table *t,uint64_t h,lamix_object **out) {
  if(!t||!t->entries)return LH_INVALID;
  uintptr_t f=enter(t);lamix_handle_entry *e=find(t,h);if(!e){leave(t,f);return LH_BADF;}
  lamix_object *o=e->object;e->object=0;e->rights=0;if(e->generation==UINT32_MAX)e->retired=1;else ++e->generation;
  if(out)*out=o;
  leave(t,f);if(!out)lamix_object_release(o);return LH_OK;
}
lamix_handle_status lamix_handle_close(lamix_handle_table *t,uint64_t h){return lamix_handle_remove(t,h,0);}
lamix_handle_status lamix_handle_duplicate(lamix_handle_table *t,uint64_t h,uint32_t rights,uint64_t *out) {
  if(!rights||(rights&~LAMIX_RIGHT_ALL))return LH_INVALID;
  lamix_object *o=0;lamix_handle_status s=lamix_handle_lookup(t,h,0,0,&o);if(s!=LH_OK)return s;
  uintptr_t f=enter(t);lamix_handle_entry *e=find(t,h);if(!e){leave(t,f);lamix_object_release(o);return LH_BADF;}
  if(rights&~e->rights){leave(t,f);lamix_object_release(o);return LH_ACCESS;}leave(t,f);
  s=lamix_handle_install(t,o,rights,out);lamix_object_release(o);return s;
}
lamix_handle_status lamix_handle_transfer_batch(lamix_handle_table *t,
  const uint64_t *handles,const uint32_t *rights,const uint8_t *move,
  lamix_object *const *expected,size_t count,lamix_object **objects) {
  if(!t||!t->entries||(!handles&&count)||(!rights&&count)||(!move&&count)||
     (!expected&&count)||(!objects&&count))return LH_INVALID;
  uintptr_t f=enter(t);
  for(size_t i=0;i<count;i++) {
    lamix_handle_entry *e=find(t,handles[i]);
    if(!e){leave(t,f);return LH_BADF;}
    if(e->object!=expected[i]){leave(t,f);return LH_BADF;}
    if(!(e->rights&LAMIX_RIGHT_TRANSFER)||(rights[i]&~e->rights)){
      leave(t,f);return LH_ACCESS;
    }
    if(!rights[i]||(rights[i]&~LAMIX_RIGHT_ALL)||move[i]>1){leave(t,f);return LH_INVALID;}
    for(size_t j=0;j<i;j++)if(handles[i]==handles[j]){leave(t,f);return LH_INVALID;}
  }
  for(size_t i=0;i<count;i++) {
    lamix_handle_entry *e=find(t,handles[i]);
    objects[i]=e->object;
    if(move[i]) {
      e->object=0;e->rights=0;
      if(e->generation==UINT32_MAX)e->retired=1;else ++e->generation;
    } else lamix_object_retain(e->object);
  }
  leave(t,f);
  return LH_OK;
}
