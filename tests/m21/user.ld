/* SPDX-License-Identifier: GPL-3.0-only */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)
SECTIONS {
  . = 0x0000008000000000;
  .text : { *(.text.start) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
