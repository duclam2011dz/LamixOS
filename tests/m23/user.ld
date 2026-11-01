/* SPDX-License-Identifier: GPL-3.0-only */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)
PHDRS {
  text PT_LOAD FLAGS(5);
  rodata PT_LOAD FLAGS(4);
  data PT_LOAD FLAGS(6);
  dynamic PT_DYNAMIC FLAGS(6);
}
SECTIONS {
  . = 0x1000;
  .text : { *(.text.start) *(.text*) } :text
  . = ALIGN(0x1000);
  .rodata : { *(.rodata*) } :rodata
  . = ALIGN(0x1000);
  .data : { *(.data*) } :data
  .dynamic : { *(.dynamic) } :data :dynamic
  .got : { *(.got*) } :data
  .rela.dyn : { *(.rela.dyn) } :data
  . = ALIGN(0x1000);
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } :data
  . = ALIGN(0x1000);
  /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) *(.interp) *(.plt*) }
}
