# Bảng layout LBP v1

Mọi offset tính bằng byte; structs căn 8 byte. Header C là nguồn assertions.

## lbp_header

| Trường | Offset | Size |
|---|---:|---:|
| `magic` | 0 | 8 |
| `major` | 8 | 2 |
| `minor` | 10 | 2 |
| `header_size` | 12 | 4 |
| `total_size` | 16 | 8 |
| `required_features` | 24 | 8 |
| `provided_features` | 32 | 8 |
| `records_offset` | 40 | 8 |
| `record_count` | 48 | 4 |
| `reserved` | 52 | 4 |
| `self_phys` | 56 | 8 |

Size: 64.

## lbp_record

| Trường | Offset | Size |
|---|---:|---:|
| `type` | 0 | 4 |
| `flags` | 4 | 4 |
| `size` | 8 | 8 |

Size: 16.

## lbp_table

| Trường | Offset | Size |
|---|---:|---:|
| `record` | 0 | 16 |
| `version` | 16 | 4 |
| `stride` | 20 | 4 |
| `count` | 24 | 8 |

Size: 32.

## lbp_memory

| Trường | Offset | Size |
|---|---:|---:|
| `base` | 0 | 8 |
| `length` | 8 | 8 |
| `type` | 16 | 4 |
| `flags` | 20 | 4 |
| `firmware_attributes` | 24 | 8 |

Size: 32.

## lbp_kernel

| Trường | Offset | Size |
|---|---:|---:|
| `record` | 0 | 16 |
| `entry` | 16 | 8 |
| `version` | 24 | 4 |
| `stride` | 28 | 4 |
| `count` | 32 | 8 |

Size: 40.

## lbp_segment

| Trường | Offset | Size |
|---|---:|---:|
| `physical` | 0 | 8 |
| `virtual_address` | 8 | 8 |
| `length` | 16 | 8 |
| `permissions` | 24 | 4 |
| `reserved` | 28 | 4 |

Size: 32.

## lbp_reservation

| Trường | Offset | Size |
|---|---:|---:|
| `base` | 0 | 8 |
| `length` | 8 | 8 |
| `purpose` | 16 | 4 |
| `release` | 20 | 4 |
| `reserved` | 24 | 8 |

Size: 32.

## lbp_entry

| Trường | Offset | Size |
|---|---:|---:|
| `record` | 0 | 16 |
| `cr3` | 16 | 8 |
| `stack_physical` | 24 | 8 |
| `stack_size` | 32 | 8 |
| `rsp` | 40 | 8 |
| `guard` | 48 | 8 |
| `version` | 56 | 4 |
| `stride` | 60 | 4 |
| `count` | 64 | 8 |

Size: 72.

## lbp_device

| Trường | Offset | Size |
|---|---:|---:|
| `record` | 0 | 16 |
| `source` | 16 | 4 |
| `block_size` | 20 | 4 |
| `disk_guid` | 24 | 16 |
| `esp_guid` | 40 | 16 |
| `system_guid` | 56 | 16 |
| `slot` | 72 | 4 |
| `reserved` | 76 | 4 |

Size: 80.

## lbp_module

| Trường | Offset | Size |
|---|---:|---:|
| `physical` | 0 | 8 |
| `length` | 8 | 8 |
| `name_offset` | 16 | 8 |
| `name_length` | 24 | 4 |
| `reserved` | 28 | 4 |

Size: 32.

## lbp_framebuffer

| Trường | Offset | Size |
|---|---:|---:|
| `record` | 0 | 16 |
| `physical` | 16 | 8 |
| `length` | 24 | 8 |
| `width` | 32 | 4 |
| `height` | 36 | 4 |
| `pitch` | 40 | 4 |
| `bpp` | 44 | 4 |
| `red_mask` | 48 | 4 |
| `green_mask` | 52 | 4 |
| `blue_mask` | 56 | 4 |
| `reserved_mask` | 60 | 4 |

Size: 64.

## lbp_acpi

| Trường | Offset | Size |
|---|---:|---:|
| `record` | 0 | 16 |
| `physical` | 16 | 8 |
| `length` | 24 | 4 |
| `revision` | 28 | 4 |

Size: 32.

## lbp_note

| Trường | Offset | Size |
|---|---:|---:|
| `magic` | 0 | 8 |
| `major` | 8 | 2 |
| `minimum_minor` | 10 | 2 |
| `reserved` | 12 | 4 |
| `required_features` | 16 | 8 |
| `optional_features` | 24 | 8 |

Size: 32.
