/* K4S51153 SDRAM on FMC bank 1 (SDNE0), configured as 8 MB x16 at 0xC0000000. */
#ifndef SDRAM_H
#define SDRAM_H
#include <stdint.h>
#define SDRAM_BASE   0xC0000000u
#define SDRAM_BYTES  (8u * 1024u * 1024u)
int  sdram_init(void);            /* configure FMC, run a quick self-test; 0 = usable */
int  sdram_ok(void);
int  sdram_test_full(void);       /* slow (~1 s) whole-chip test, for diagnostics */
#endif
