/* Algoradix PATCH HOST (STAGE 5) — STM32F427IGH6 memory layout.
 * Differs from STM32F427xG.ld: the first 64 KB of SRAM1 are the patch's (run through the address-0 remap),
 * the host's own data/bss/DMA buffers live in SRAM3 (speed does not matter for them).
 *
 * flash : 1 MB. We use the first 896 KB (sectors 0-10) for code and keep
 *         sector 11 (128 KB at 0x080E0000) for presets / calibration.
 * sram  : SRAM1 (112 KB) + SRAM2 (16 KB) + SRAM3 (64 KB) are contiguous = 192 KB.
 *         Split into data/bss/DMA (96 KB), main stack (32 KB, must be below
 *         0x20020000 for the ROM bootloader jump check) and SRAM3 buffers.
 * ccm   : 64 KB core-coupled memory, fast scratch, not DMA-reachable.
 */
MEMORY
{
  flash (rx)  : ORIGIN = 0x08000000, LENGTH = 896K
  sram  (rwx) : ORIGIN = 0x20020000, LENGTH = 64K      /* SRAM3: host data, bss, DMA buffers */
  stack (rw)  : ORIGIN = 0x20018000, LENGTH = 0x7FF8   /* top of SRAM1/2: main stack, top = 0x2001FFF8 */
  sram3 (rwx) : ORIGIN = 0x20000000, LENGTH = 64K      /* SRAM1, first 64 KB: the loaded patch (alias 0x00000000), or the built-in engine's pool (.sram3) */
  ccm   (rw)  : ORIGIN = 0x10000000, LENGTH = 64K      /* CCM: fast scratch (.ccm), no DMA */
}

/* The ROM DFU bootloader only jumps to an image whose initial SP satisfies
 * (SP & 0x2FFE0000) == 0x20000000, i.e. lies in 0x20000000..0x2001FFFF.
 * A CCM stack (what Axoloti used) makes "dfu-util ... :leave" silently fail. */
_stack_top      = ORIGIN(stack) + LENGTH(stack);
_presets_flash  = 0x080E0000;
_presets_size   = 128K;

ENTRY(Reset_Handler)

SECTIONS
{
  .isr_vector : {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > flash

  .text : {
    . = ALIGN(4);
    *(.text .text.*)
    *(.rodata .rodata.*)
    *(.glue_7 .glue_7t)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
  } > flash

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > flash
  .ARM.exidx : {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  } > flash

  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)); PROVIDE_HIDDEN(__preinit_array_end = .); } > flash
  .init_array    : { PROVIDE_HIDDEN(__init_array_start = .);    KEEP(*(SORT(.init_array.*))); KEEP(*(.init_array*)); PROVIDE_HIDDEN(__init_array_end = .); } > flash

  . = ALIGN(4);
  _sidata = LOADADDR(.data);

  .data : {
    . = ALIGN(4);
    _sdata = .;
    *(.data .data.*)
    . = ALIGN(4);
    _edata = .;
  } > sram AT > flash

  .bss (NOLOAD) : {
    . = ALIGN(4);
    _sbss = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > sram

  /* DMA buffers: NOLOAD, in SRAM, zeroed by startup together with .bss? No:
     kept separate so the section stays 32-byte aligned and obviously DMA-safe. */
  .dma (NOLOAD) : {
    . = ALIGN(32);
    _sdma = .;
    *(.dma .dma.*)
    . = ALIGN(32);
    _edma = .;
  } > sram

  .noinit (NOLOAD) : {
    . = ALIGN(4);
    *(.noinit .noinit.*)
    . = ALIGN(4);
  } > sram

  _end = .;
  PROVIDE(end = .);

  .sram3 (NOLOAD) : {
    . = ALIGN(32);
    *(.sram3 .sram3.*)
    . = ALIGN(32);
  } > sram3

  .ccm (NOLOAD) : {
    . = ALIGN(8);
    *(.ccm .ccm.*)
    . = ALIGN(8);
    _eccm = .;
  } > ccm

  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) }
}
