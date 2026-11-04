/* One shared memory pool for the active patch's voices and tables: only one
 * patch runs at a time, so their state overlays. Reset by the framework
 * before a patch's init(). */
#ifndef POOL_H
#define POOL_H
#include <stddef.h>
#ifndef POOL_BYTES
#if defined(APX_PATCH) && !defined(SIM)
#define POOL_BYTES (20 * 1024)   /* inside the patch's 64 KB: voices. Anything big goes to SDRAM through the host */
#else
#define POOL_BYTES (60 * 1024)
#endif
#endif
#define POOL_BIG_BYTES (6 * 1024) /* APX builds: allocations from this size up come from the host's SDRAM */
void  pool_reset(void);
void *pool_alloc(size_t bytes);      /* zeroed, 32-byte aligned; NULL if the pool is exhausted */
size_t pool_used(void);
void  pool_reset_extern(void);  /* same as pool_reset; separate name so the instrument adapter does not need pool.h's size macros */
#endif
