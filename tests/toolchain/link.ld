/* SPDX-License-Identifier: GPL-3.0-only */
/* Địa chỉ chỉ cho sample M01, không đóng băng layout OS. */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(m01_probe_entry)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
  . = 0x100000;
  .text : { *(.text*) } :text
  .rodata : { *(.rodata*) } :text
  . = ALIGN(4096);
  .data : { *(.data*) } :data
  .bss : { *(.bss*) *(COMMON) } :data
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
