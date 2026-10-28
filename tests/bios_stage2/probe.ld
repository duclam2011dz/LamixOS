/* SPDX-License-Identifier: GPL-3.0-only */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(probe_entry)
PHDRS {
  text PT_LOAD FLAGS(5);
  rodata PT_LOAD FLAGS(4);
  data PT_LOAD FLAGS(6);
  note PT_NOTE FLAGS(4);
  stack PT_GNU_STACK FLAGS(6);
}
SECTIONS {
  . = 0xffffffff80000000;
  .text : { KEEP(*(.text.entry)) *(.text*) } :text
  . = ALIGN(4096);
  .rodata : { *(.rodata*) } :rodata
  .note.lamix : { KEEP(*(.note.lamix)) } :rodata :note
  . = ALIGN(4096);
  .data : { *(.data*) } :data
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } :data
  /DISCARD/ : { *(.comment) *(.eh_frame*) }
}
