/* FatFS glue: single volume = the micro-SD (read-only build). */
#include "ff.h"
#include "diskio.h"
#include "sdio.h"

DSTATUS disk_status(BYTE pdrv) { (void)pdrv; return sd.ready ? 0 : STA_NOINIT; }
DSTATUS disk_initialize(BYTE pdrv) { (void)pdrv; return sd_init() == 0 ? 0 : STA_NOINIT; }
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
  (void)pdrv;
  if (((uint32_t)buff & 3u) == 0) return sd_read_blocks((uint32_t)sector, buff, count) == 0 ? RES_OK : RES_ERROR;
  /* unaligned buffer: go one block at a time through an aligned bounce buffer */
  static uint8_t bounce[512] __attribute__((aligned(4)));
  for (UINT i = 0; i < count; i++) {
    if (sd_read_blocks((uint32_t)sector + i, bounce, 1) != 0) return RES_ERROR;
    for (int k = 0; k < 512; k++) buff[i * 512 + k] = bounce[k];
  }
  return RES_OK;
}
#if !FF_FS_READONLY
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) { (void)pdrv; (void)buff; (void)sector; (void)count; return RES_WRPRT; }
#endif
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
  (void)pdrv;
  switch (cmd) {
  case CTRL_SYNC: return RES_OK;
  case GET_SECTOR_COUNT: *(LBA_t *)buff = sd.blocks; return RES_OK;
  case GET_SECTOR_SIZE: *(WORD *)buff = 512; return RES_OK;
  case GET_BLOCK_SIZE: *(DWORD *)buff = 1; return RES_OK;
  default: return RES_PARERR;
  }
}
