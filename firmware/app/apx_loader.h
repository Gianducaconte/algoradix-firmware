/* Patch host: the banks and the list of .APX files on the card, loading one into SRAM1, and the services a patch gets.
 * Main loop only. Card layout: /ALGORADIX/PATCHES/<file>.APX = the bank "Core", /ALGORADIX/PATCHES/<folder>/<file>.APX =
 * one more bank per folder (Bank page order = folder-name order; a leading number and separator are not shown:
 * "3-Pad" is shown as "Pad"). Patch page order = file-name order inside the bank. Each patch's own data in
 * /ALGORADIX/<NAME without spaces>/, whatever its bank. */
#ifndef APX_LOADER_H
#define APX_LOADER_H
#include "apx.h"
#define APX_LIST_MAX 128                          /* patches per bank */
#define APX_BANK_MAX 16
typedef struct { char name[16]; char path[64]; uint32_t bytes, hash; } apx_entry_t;
typedef struct { char name[16]; char dir[40]; } apx_bank_t;
extern apx_entry_t apx_list[APX_LIST_MAX];        /* the patches of ONE bank: the one apx_scan() was called for */
extern int apx_count;
extern apx_bank_t apx_banks[APX_BANK_MAX];
extern int apx_bank_count;
int  apx_scan_banks(void);                        /* (re)build the bank list; returns the count (0 = no PATCHES folder) */
int  apx_scan(int bank);                          /* (re)build the patch list of that bank; returns the count */
int  apx_bank_of(uint32_t hash);                /* the bank holding the patch file with this hash (names only, no file is opened), or -1 */
int  apx_find_hash(uint32_t h);                   /* index or -1 */
/* stage = read + verify into a staging buffer while the old patch still plays; 0 = ok, < 0 = error (see apx_error) */
int  apx_stage(int index);
extern const char *apx_error;                     /* short word for the display */
/* commit = copy the staged image to its load address and clear its bss. Only while the audio ISR is silent. */
const apx_header_t *apx_commit(void);
const apx_host_t *apx_host_api(const char *patch_name, void (*progress)(const char *, int));   /* also resets the patch's SDRAM heap */
#endif
