/* SPDX-License-Identifier: GPL-3.0-only */
OUTPUT_FORMAT(elf32-i386)
ENTRY(stage2_core_entry)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
  . = 0x20000;
  __core_start = .;
  .text : { KEEP(*(.text.entry)) *(.text*) } :text
  .rodata : { *(.rodata*) } :text
  . = ALIGN(4096);
  __core_writable = .;
  .data : { *(.data*) } :data
  __core_file_end = .;
  .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) }
  . = ALIGN(4096);
  __core_end = .;
  /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
  ASSERT(__core_end <= 0x60000, "M05 core memory exceeds 256 KiB")
}
