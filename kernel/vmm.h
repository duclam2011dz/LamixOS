/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_KERNEL_VMM_H
#define LAMIX_KERNEL_VMM_H

#include <stdint.h>

#define VMM_PAGE_SIZE UINT64_C(4096)
#define VMM_PHYSICAL_LIMIT (UINT64_C(1) << 52)

#define VMM_READ UINT32_C(1)
#define VMM_WRITE UINT32_C(2)
#define VMM_EXECUTE UINT32_C(4)
#define VMM_USER UINT32_C(8)
#define VMM_MAP_UNCACHED UINT32_C(1)

typedef enum vmm_status {
  VMM_OK,
  VMM_INVALID_ARGUMENT,
  VMM_INVALID_ADDRESS,
  VMM_OUT_OF_MEMORY,
  VMM_ALREADY_MAPPED,
  VMM_NOT_MAPPED,
  VMM_CORRUPT_TABLE,
  VMM_CALLBACK_FAILED
} vmm_status;

typedef struct vmm_operations {
  void *context;
  int (*allocate_page)(void *context, uint64_t *physical);
  int (*release_page)(void *context, uint64_t physical);
  volatile uint64_t *(*access_page)(void *context, uint64_t physical);
  void (*invalidate)(void *context, uint64_t virtual_address);
} vmm_operations;

typedef struct vmm_space {
  vmm_operations operations;
  uint64_t root_physical;
  uint32_t nx_enabled;
  uint32_t initialized;
  uint32_t mapping_permissions;
  volatile uint64_t active_cpus;
} vmm_space;

vmm_status vmm_init(vmm_space *space, const vmm_operations *operations,
                    int nx_enabled);
/* Chia sẻ các PML4 entries của kernel ngoại trừ slot 1 dành riêng cho M20 user.
 * Các entries được chia sẻ phải giữ quyền supervisor. VMM_USER chỉ nhận VA
 * trong PML4 slot 1 để vòng đời process chỉ sở hữu một nhánh root. */
vmm_status vmm_copy_kernel_mappings(vmm_space *destination,
                                    const vmm_space *kernel);
/* Chỉ hủy process root không còn nạp trong CR3; không thu hồi leaf frames. */
vmm_status vmm_destroy_process_space(vmm_space *space);
/* Chỉ hủy root không còn được nạp trong CR3; callbacks phải tiếp tục truy cập
 * được mọi page-table frame trong suốt quá trình thu hồi. */
vmm_status vmm_destroy(vmm_space *space);
vmm_status vmm_map_range(vmm_space *space, uint64_t virtual_address,
                         uint64_t physical_address, uint64_t length,
                         uint32_t permissions, uint32_t attributes);
vmm_status vmm_unmap_range(vmm_space *space, uint64_t virtual_address,
                           uint64_t length);
vmm_status vmm_protect_range(vmm_space *space, uint64_t virtual_address,
                             uint64_t length, uint32_t permissions);
vmm_status vmm_query(vmm_space *space, uint64_t virtual_address,
                     uint64_t *physical_address, uint32_t *permissions,
                     uint64_t *entry_flags);
vmm_status vmm_prepare_scratch(vmm_space *space, uint64_t data_virtual,
                               uint64_t control_virtual,
                               uint64_t *control_page_physical);

#endif
