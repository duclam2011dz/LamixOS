/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_IPC_H
#define LAMIX_KERNEL_IPC_H
#include "handle.h"
#define LAMIX_CHANNEL_DEPTH 16u
#define LAMIX_CHANNEL_MAX_PAYLOAD 256u
#define LAMIX_CHANNEL_MAX_ATTACHMENTS 4u
#define LAMIX_SHM_MAX_PAGES 16u
typedef enum lamix_ipc_status { LI_OK, LI_INVALID, LI_FAULT, LI_BADF, LI_ACCESS,
 LI_AGAIN, LI_MSGSIZE, LI_NOMEM, LI_CLOSED, LI_EXISTS, LI_TOO_MANY } lamix_ipc_status;
typedef enum lamix_attachment_disposition { LAMIX_ATTACHMENT_MOVE=1, LAMIX_ATTACHMENT_DUP=2 } lamix_attachment_disposition;
typedef struct lamix_attachment { uint64_t handle; uint32_t rights; uint8_t disposition; uint8_t reserved[3]; } lamix_attachment;
_Static_assert(sizeof(lamix_attachment)==16,"M22 attachment ABI");
typedef struct lamix_message { uint16_t length, count; uint8_t payload[LAMIX_CHANNEL_MAX_PAYLOAD]; struct {lamix_object *object;uint32_t rights;} attachments[LAMIX_CHANNEL_MAX_ATTACHMENTS]; } lamix_message;
typedef struct lamix_channel_core { volatile uint32_t references; volatile uint8_t closed[2]; uint8_t head[2],count[2]; lamix_message queue[2][LAMIX_CHANNEL_DEPTH]; } lamix_channel_core;
typedef struct lamix_channel_endpoint {lamix_object object;lamix_channel_core *core;uint8_t side;} lamix_channel_endpoint;
typedef struct lamix_ipc_ops {void *context;void *(*allocate)(void *,size_t);void (*deallocate)(void *,void *);uintptr_t (*lock)(void *);void (*unlock)(void *,uintptr_t);} lamix_ipc_ops;
typedef struct lamix_shm_mapping {struct lamix_shm_mapping *next;lamix_object *object;uint64_t address;uint32_t pages;} lamix_shm_mapping;
typedef struct lamix_shm_ops {lamix_ipc_ops memory;int (*allocate_page)(void *,uint64_t *);int (*release_page)(void *,uint64_t);volatile uint8_t *(*access_page)(void *,uint64_t);int (*map_page)(void *,uint64_t,uint64_t,uint32_t);int (*unmap_page)(void *,uint64_t);} lamix_shm_ops;
lamix_ipc_status lamix_channel_create(lamix_handle_table *,const lamix_ipc_ops *,uint64_t *,uint64_t *);
lamix_ipc_status lamix_channel_send(lamix_handle_table *,const lamix_ipc_ops *,uint64_t,const void *,size_t,const lamix_attachment *,size_t);
lamix_ipc_status lamix_channel_receive(lamix_handle_table *,const lamix_ipc_ops *,uint64_t,void *,size_t,size_t *,lamix_attachment *,size_t,size_t *);
void lamix_ipc_close_all(lamix_handle_table *);
lamix_ipc_status lamix_shm_create(lamix_handle_table *,const lamix_shm_ops *,uint32_t,uint64_t *);
lamix_ipc_status lamix_shm_map(lamix_handle_table *,const lamix_shm_ops *,lamix_shm_mapping **,uint64_t,uint64_t,uint32_t);
lamix_ipc_status lamix_shm_unmap(const lamix_shm_ops *,lamix_shm_mapping **,uint64_t);
#endif
