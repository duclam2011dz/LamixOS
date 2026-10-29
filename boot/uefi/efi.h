/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMIX_UEFI_EFI_H
#define LAMIX_UEFI_EFI_H
#include <stddef.h>
#include <stdint.h>
typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;
typedef uint16_t CHAR16;
typedef struct {
  uint32_t a;
  uint16_t b, c;
  uint8_t d[8];
} EFI_GUID;
typedef struct {
  uint64_t signature;
  uint32_t revision, header_size, crc32, reserved;
} EFI_TABLE_HEADER;
typedef struct EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef EFI_STATUS(__attribute__((ms_abi)) *
                   EFI_ALLOCATE_PAGES)(uint32_t, uint32_t, uint64_t,
                                       uint64_t *);
typedef EFI_STATUS(__attribute__((ms_abi)) * EFI_FREE_PAGES)(uint64_t,
                                                             uint64_t);
typedef EFI_STATUS(__attribute__((ms_abi)) *
                   EFI_GET_MEMORY_MAP)(uint64_t *, void *, uint64_t *,
                                       uint64_t *, uint32_t *);
typedef EFI_STATUS(__attribute__((ms_abi)) *
                   EFI_ALLOCATE_POOL)(uint32_t, uint64_t, void **);
typedef EFI_STATUS(__attribute__((ms_abi)) * EFI_FREE_POOL)(void *);
typedef EFI_STATUS(__attribute__((ms_abi)) *
                   EFI_HANDLE_PROTOCOL)(EFI_HANDLE, EFI_GUID *, void **);
typedef EFI_STATUS(__attribute__((ms_abi)) * EFI_EXIT_BOOT_SERVICES)(EFI_HANDLE,
                                                                     uint64_t);
typedef EFI_STATUS(__attribute__((ms_abi)) *
                   EFI_LOCATE_PROTOCOL)(EFI_GUID *, void *, void **);
typedef EFI_STATUS(__attribute__((ms_abi)) *
                   EFI_LOCATE_HANDLE_BUFFER)(uint32_t, EFI_GUID *, void *,
                                             uint64_t *, EFI_HANDLE **);
typedef struct EFI_BOOT_SERVICES {
  EFI_TABLE_HEADER h;
  void *raise_tpl, *restore_tpl;
  EFI_ALLOCATE_PAGES allocate_pages;
  EFI_FREE_PAGES free_pages;
  EFI_GET_MEMORY_MAP get_memory_map;
  EFI_ALLOCATE_POOL allocate_pool;
  EFI_FREE_POOL free_pool;
  void *rest[9];
  EFI_HANDLE_PROTOCOL handle_protocol;
  void *rest1[9];
  EFI_EXIT_BOOT_SERVICES exit_boot_services;
  void *rest2[9];
  EFI_LOCATE_HANDLE_BUFFER locate_handle_buffer;
  EFI_LOCATE_PROTOCOL locate_protocol;
} EFI_BOOT_SERVICES;
typedef struct {
  uint16_t year;
  uint8_t month, day, hour, minute, second, pad1;
  uint32_t ns;
  int16_t tz;
  uint8_t daylight, pad2;
} EFI_TIME;
typedef struct {
  EFI_GUID vendor_guid;
  void *vendor_table;
} EFI_CONFIGURATION_TABLE;
struct EFI_SYSTEM_TABLE {
  EFI_TABLE_HEADER h;
  CHAR16 *vendor;
  uint32_t revision;
  EFI_HANDLE console_in_handle;
  void *console_in;
  EFI_HANDLE console_out_handle;
  void *console_out;
  EFI_HANDLE stderr_handle;
  void *stderr;
  void *runtime;
  EFI_BOOT_SERVICES *boot;
  uint64_t table_count;
  EFI_CONFIGURATION_TABLE *tables;
};
_Static_assert(offsetof(EFI_BOOT_SERVICES, handle_protocol) == 152,
               "UEFI HandleProtocol offset");
_Static_assert(offsetof(EFI_BOOT_SERVICES, exit_boot_services) == 232,
               "UEFI ExitBootServices offset");
_Static_assert(offsetof(EFI_BOOT_SERVICES, locate_handle_buffer) == 312,
               "UEFI LocateHandleBuffer offset");
_Static_assert(offsetof(EFI_BOOT_SERVICES, locate_protocol) == 320,
               "UEFI LocateProtocol offset");
_Static_assert(offsetof(EFI_SYSTEM_TABLE, boot) == 96,
               "UEFI system table BootServices offset");
_Static_assert(offsetof(EFI_SYSTEM_TABLE, tables) == 112,
               "UEFI system table config table offset");
typedef struct {
  uint32_t revision;
  EFI_HANDLE parent, system_table, device_handle;
  void *file_path;
  uint32_t options_size;
  void *options;
  void *image_base;
  uint64_t image_size;
  uint32_t code_type, data_type;
  void *unload;
} EFI_LOADED_IMAGE;
typedef struct EFI_FILE EFI_FILE;
struct EFI_FILE {
  uint64_t revision;
  void *open, *close, *delete;
  EFI_STATUS(__attribute__((ms_abi)) * read)(EFI_FILE *, uint64_t *, void *);
  void *write, *get_position, *set_position, *get_info, *set_info, *flush,
      *open_ex, *read_ex, *write_ex, *flush_ex;
};
typedef struct {
  uint64_t revision;
  EFI_STATUS(__attribute__((ms_abi)) * open_volume)(void *, EFI_FILE **);
} EFI_SIMPLE_FS;
typedef struct {
  uint32_t type, pad;
  uint64_t physical_start, virtual_start, pages, attributes;
} EFI_MEMORY_DESCRIPTOR;
_Static_assert(sizeof(EFI_MEMORY_DESCRIPTOR) == 40,
               "UEFI memory descriptor prefix size");
_Static_assert(offsetof(EFI_MEMORY_DESCRIPTOR, physical_start) == 8,
               "UEFI memory descriptor physical start");
_Static_assert(offsetof(EFI_MEMORY_DESCRIPTOR, pages) == 24,
               "UEFI memory descriptor page count");
_Static_assert(offsetof(EFI_MEMORY_DESCRIPTOR, attributes) == 32,
               "UEFI memory descriptor attributes");
#define EFI_ERROR(s) (((s) >> 63) != 0)
#define EFI_SUCCESS 0
#define EFI_BUFFER_TOO_SMALL UINT64_C(0x8000000000000005)
#define EFI_INVALID_PARAMETER UINT64_C(0x8000000000000002)
#define EFI_ALLOCATE_ANY_PAGES 0u
#define EFI_ALLOCATE_MAX_ADDRESS 1u
#define EFI_LOADER_DATA 2u
#define EFI_FILE_MODE_READ 1u
extern EFI_GUID g_loaded_image_guid, g_simple_fs_guid, g_gop_guid;
#endif
