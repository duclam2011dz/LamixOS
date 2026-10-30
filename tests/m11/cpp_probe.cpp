// SPDX-License-Identifier: GPL-3.0-only
#include "heap.h"

extern "C" int m11_cpp_heap_probe(kheap *heap) {
  void *allocation = kheap_alloc(heap, 37);
  if (allocation == nullptr)
    return 0;
  return kheap_free(heap, allocation) == KHEAP_OK;
}
