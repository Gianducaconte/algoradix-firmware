/* ADC1 continuous scan of the 9 panel channels into adc_raw[] via DMA2 stream 0.
 * 12-bit, 0..4095, VDDA reference. The buffer is always current. */
#ifndef ADC_PANEL_H
#define ADC_PANEL_H
#include <stdint.h>
#include "board_algoradix.h"
extern volatile uint16_t adc_raw[ADC_NUM_CHANNELS];
void adc_panel_init(void);
#endif
