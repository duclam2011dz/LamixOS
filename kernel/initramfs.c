/* SPDX-License-Identifier: GPL-3.0-only */
#include "initramfs.h"

static int hex_field(const uint8_t *p, size_t n, uint32_t *value) {
  uint32_t out = 0;
  for (size_t i = 0; i < n; ++i) {
    uint8_t c = p[i];
    uint32_t digit = c >= '0' && c <= '9' ? (uint32_t)(c - '0')
                      : c >= 'a' && c <= 'f' ? (uint32_t)(c - 'a' + 10)
                      : c >= 'A' && c <= 'F' ? (uint32_t)(c - 'A' + 10)
                                             : 16;
    if (digit > 15 || out > (UINT32_MAX - digit) / 16)
      return 0;
    out = out * 16 + digit;
  }
  *value = out;
  return 1;
}

static int align4(size_t n, size_t *out) {
  if (n > SIZE_MAX - 3)
    return 0;
  *out = (n + 3) & ~(size_t)3;
  return 1;
}

lamix_initramfs_status lamix_initramfs_find(const void *archive,
                                             size_t archive_size,
                                             const char *name,
                                             lamix_initramfs_file *file) {
  const uint8_t *bytes = archive;
  size_t offset = 0;
  const uint8_t *found_data = 0;
  size_t found_size = 0;
  if (!bytes || !name || !*name || !file)
    return LAMIX_INITRAMFS_INVALID;
  while (offset < archive_size) {
    uint32_t namesize, filesize, mode;
    size_t header_end, name_end, data_offset, data_end, next;
    if (archive_size - offset < 110)
      return LAMIX_INITRAMFS_INVALID;
    const uint8_t *h = bytes + offset;
    if ((h[0] != '0' || h[1] != '7' || h[2] != '0' || h[3] != '7' ||
         h[4] != '0' || h[5] != '1') ||
        !hex_field(h + 14, 8, &mode) ||
        !hex_field(h + 54, 8, &filesize) ||
        !hex_field(h + 94, 8, &namesize) || namesize < 1)
      return LAMIX_INITRAMFS_INVALID;
    if (offset > SIZE_MAX - 110)
      return LAMIX_INITRAMFS_INVALID;
    header_end = offset + 110;
    if (namesize > archive_size - header_end)
      return LAMIX_INITRAMFS_INVALID;
    name_end = header_end + namesize;
    if (bytes[name_end - 1] != 0 || !align4(name_end, &data_offset) ||
        data_offset > archive_size || filesize > archive_size - data_offset)
      return LAMIX_INITRAMFS_INVALID;
    data_end = data_offset + filesize;
    if (!align4(data_end, &next) || next > archive_size)
      return LAMIX_INITRAMFS_INVALID;
    int trailer = namesize == sizeof("TRAILER!!!") &&
                  bytes[header_end] == 'T';
    if (trailer) {
      static const char trailer_name[] = "TRAILER!!!";
      for (size_t i = 0; i < sizeof(trailer_name); ++i)
        if (bytes[header_end + i] != (uint8_t)trailer_name[i])
          return LAMIX_INITRAMFS_INVALID;
      if (filesize)
        return LAMIX_INITRAMFS_INVALID;
      for (size_t i = next; i < archive_size; ++i)
        if (bytes[i])
          return LAMIX_INITRAMFS_INVALID;
      if (!found_data)
        return LAMIX_INITRAMFS_NOT_FOUND;
      file->data = found_data;
      file->size = found_size;
      return LAMIX_INITRAMFS_OK;
    }
    if ((mode & 0170000u) != 0100000u)
      return LAMIX_INITRAMFS_INVALID;
    size_t wanted = 0;
    while (name[wanted]) {
      if (wanted + 1 >= namesize)
        break;
      ++wanted;
    }
    if (name[wanted] == 0 && wanted + 1 == namesize) {
      size_t i = 0;
      while (i < wanted && bytes[header_end + i] == (uint8_t)name[i])
        ++i;
      if (i == wanted) {
        if ((mode & 0170000u) != 0100000u || found_data)
          return LAMIX_INITRAMFS_INVALID;
        found_data = bytes + data_offset;
        found_size = filesize;
      }
    }
    offset = next;
  }
  return LAMIX_INITRAMFS_INVALID;
}
