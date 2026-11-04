/* Linker script for an APX patch: one flat image at the fixed load address (SRAM1 through the address-0 remap),
 * header first, then code, constants and initialised data; bss follows and is cleared by the host. */
MEMORY { ram (rwx) : ORIGIN = 0x00000400, LENGTH = 63K }   /* SRAM1 remapped to 0: fast I/D-bus access (see apx.h) */
ENTRY(apx_header)
SECTIONS
{
  .image : {
    KEEP(*(.apx_header))
    . = ALIGN(4);
    *(.text .text.*)
    *(.glue_7 .glue_7t)
    *(.rodata .rodata.*)
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
    . = ALIGN(4);
    *(.data .data.*)
    . = ALIGN(4);
  } > ram
  .bss (NOLOAD) : {
    . = ALIGN(32);
    _apx_bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(32);
    _apx_bss_end = .;
  } > ram
  /DISCARD/ : { *(.note.GNU-stack) *(.comment) *(.ARM.attributes) }
}
