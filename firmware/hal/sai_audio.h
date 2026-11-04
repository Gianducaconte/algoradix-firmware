/* SAI1 audio transport: block A = slave transmitter (to DAC), block B =
 * synchronous slave receiver (from ADC). 32-bit words, 2 slots (L,R),
 * DMA double-buffered, AUDIO_BLOCK stereo frames per interrupt.
 *
 * The callback runs in the DMA interrupt (priority 1) every 333 us with
 * in[2*AUDIO_BLOCK] (L,R interleaved, 24-bit left-justified in int32) and
 * must fill out[2*AUDIO_BLOCK] the same way. */
#ifndef SAI_AUDIO_H
#define SAI_AUDIO_H
#include <stdint.h>
#include "board_algoradix.h"
typedef void (*audio_cb_t)(const int32_t *in, int32_t *out, int frames);
void     sai_audio_start(audio_cb_t cb);
uint32_t sai_audio_blocks(void);        /* interrupt counter, for diagnostics */
uint32_t sai_audio_load_permille(void); /* time spent in callback, 0..1000 */
#endif
