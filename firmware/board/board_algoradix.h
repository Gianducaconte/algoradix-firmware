/* Algoradix — every pin the firmware touches, in one place.
 * Axoloti Core v1.2 (STM32F427IGH6) + Algoradix interface board.
 * See docs/hardware-facts.md for the derivation.
 */
#ifndef BOARD_ALGORADIX_H
#define BOARD_ALGORADIX_H

#include "stm32f4xx.h"

/* ---- clocks ---------------------------------------------------------- */
#define HSE_HZ            8000000u
#define SYSCLK_HZ       168000000u
#define APB1_HZ          42000000u
#define APB2_HZ          84000000u

/* ---- panel: analog (ADC1) ------------------------------------------- */
#define POT_F1_PORT      GPIOA
#define POT_F1_PIN       5      /* ADC1_IN5,  + CV */
#define POT_F2_PORT      GPIOA
#define POT_F2_PIN       3      /* ADC1_IN3,  + CV */
#define POT_TUNE_PORT    GPIOC
#define POT_TUNE_PIN     4      /* ADC1_IN14, + CV */
#define POT_OCT_PORT     GPIOC
#define POT_OCT_PIN      1      /* ADC1_IN11 */
#define POT_A_PORT       GPIOC
#define POT_A_PIN        2      /* ADC1_IN12 */
#define POT_D_PORT       GPIOC
#define POT_D_PIN        3      /* ADC1_IN13, + CV */
#define POT_FILT_PORT    GPIOB
#define POT_FILT_PIN     0      /* ADC1_IN8,  + CV */
#define POT_LEVELS_PORT  GPIOC
#define POT_LEVELS_PIN   0      /* ADC1_IN10 */
#define CV_ASSIGN_PORT   GPIOA
#define CV_ASSIGN_PIN    7      /* ADC1_IN7,  CV only */

enum adc_index {
  ADC_F1 = 0, ADC_F2, ADC_TUNE, ADC_OCT, ADC_A, ADC_D, ADC_FILT, ADC_LEVELS, ADC_ASSIGN,
  ADC_NUM_CHANNELS
};
#define ADC_CHANNEL_LIST  { 5, 3, 14, 11, 12, 13, 8, 10, 7 }
/* Measured 2026-09-08: every pot reads ~4080 fully LEFT and 0 fully RIGHT,
 * i.e. the wipers are wired inverted. pot_value = POT_RAW_MAX - raw. */
#define POT_INVERTED     1
#define POT_RAW_MAX      4080
/* CV jacks, measured 2026-09-08 (raw ADC counts, before pot inversion):
 *   - the CV path is INVERTING: +1 V at the jack lowers the raw value by ~400
 *   - ASSIGN idle (0 V) = 2222; a gate high gave 190 (=> ~5 V gate)
 *   - TUNE: two notes one octave apart differed by 400 counts => 400 counts/V
 *   - on pot+CV pins the CV simply adds to the (inverted) pot reading
 * So, in "pot units" (POT_RAW_MAX - raw): +1 V = +400. 1 V/oct: 400 per octave,
 * 33.3 per semitone. Raw noise ~ +/-5 counts => filter before use. */
#define CV_ASSIGN_IDLE_RAW   2222
#define CV_COUNTS_PER_VOLT   400
#define CV_GATE_ON_RAW       1800   /* raw below this  = gate high (> ~1 V) */
#define CV_GATE_OFF_RAW      2000   /* raw above this  = gate low  (hysteresis) */

/* ---- panel: digital inputs ------------------------------------------ */
#define BTN_GATE_PORT    GPIOB
#define BTN_GATE_PIN     6
#define ENC_A_PORT       GPIOC
#define ENC_A_PIN        5
#define ENC_B_PORT       GPIOA
#define ENC_B_PIN        2
#define ENC_PUSH_PORT    GPIOB
#define ENC_PUSH_PIN     7
/* Measured with Stage 0 (2026-09-08): GATE and encoder push read HIGH when
 * pressed and did not respond with pull-ups, so they connect to 3.3 V:
 * active HIGH, internal pull-DOWN. Encoder A/B work with pull-ups (to GND). */
#define BUTTONS_ACTIVE_LOW 0
#define BUTTON_PULL        GPIO_PULLDOWN
#define ENCODER_PULL       GPIO_PULLUP
/* Measured: 2 quadrature transitions per detent click; input.c's half-step state
 * machine turns them into exactly one count per click. */
#define ENC_COUNTS_PER_DETENT 1

/* ---- panel: LEDs (ring around encoder) ------------------------------ */
#define LED1_PORT        GPIOA
#define LED1_PIN         6      /* TIM3_CH1 AF2 */
#define LED2_PORT        GPIOA
#define LED2_PIN         0      /* TIM2_CH1 AF1 / TIM5_CH1 AF2 */
#define LED3_PORT        GPIOA
#define LED3_PIN         1      /* TIM2_CH2 AF1 / TIM5_CH2 AF2 */
#define LED4_PORT        GPIOA
#define LED4_PIN         4      /* no timer: soft PWM */
#define NUM_PANEL_LEDS   4
/* Ring positions measured with Stage 0: LED1 top-left, LED2 top-right,
 * LED3 bottom-left, LED4 bottom-right. Panel labels by position: */
#define LED_ALGO         0   /* LED1, top-left     */
#define LED_CLOCKVCA     1   /* LED2, top-right    */
#define LED_PATCH        2   /* LED3, bottom-left  */
#define LED_GATE         3   /* LED4, bottom-right */

/* ---- panel: OLED SSD1306 on I2C1 ------------------------------------ */
#define OLED_SCL_PORT    GPIOB
#define OLED_SCL_PIN     8      /* AF4 open-drain */
#define OLED_SDA_PORT    GPIOB
#define OLED_SDA_PIN     9      /* AF4 open-drain */
#define OLED_I2C_ADDR    0x3C
/* Measured 2026-09-08: 0.49" module, 64x32 pixels. The SSD1306 RAM is 128
 * columns wide; these modules use columns 32..95. 4 text rows of 10 chars. */
#define OLED_W           64
#define OLED_H           32
#define OLED_COL_OFFSET  32

/* ---- Core: LEDs and buttons ----------------------------------------- */
#define LED_GREEN_PORT   GPIOG
#define LED_GREEN_PIN    6      /* "LED1" / panel "I"?  active high */
#define LED_RED_PORT     GPIOC
#define LED_RED_PIN      6      /* "LED2" / panel "II"? active high */
#define SW2_PORT         GPIOA
#define SW2_PIN          10     /* to VDD per schematic, but reads HIGH at idle with USB plugged (USB ID line). Not used for control. */

/* ---- Core: codec ADAU1961 (SAI1 + I2C3) ----------------------------- */
#define CODEC_MCLK_PORT  GPIOA
#define CODEC_MCLK_PIN   8      /* MCO1 = HSE 8 MHz, AF0 */
#define CODEC_SCK_PORT   GPIOE
#define CODEC_SCK_PIN    5      /* SAI1_SCK_A AF6 (codec master) */
#define CODEC_FS_PORT    GPIOE
#define CODEC_FS_PIN     4      /* SAI1_FS_A  AF6 */
#define CODEC_SDA_PORT   GPIOE
#define CODEC_SDA_PIN    6      /* SAI1_SD_A  AF6  codec ADC -> MCU */
#define CODEC_SDB_PORT   GPIOE
#define CODEC_SDB_PIN    3      /* SAI1_SD_B  AF6  MCU -> codec DAC */
#define CODEC_I2C_SCL_PORT GPIOH
#define CODEC_I2C_SCL_PIN  7    /* I2C3_SCL AF4 open-drain */
#define CODEC_I2C_SDA_PORT GPIOH
#define CODEC_I2C_SDA_PIN  8    /* I2C3_SDA AF4 open-drain */
#define CODEC_I2C_ADDR   0x38   /* 7-bit */
#define AUDIO_FS_HZ      48000u
#define AUDIO_BLOCK      16     /* stereo frames per DMA half */

/* ---- Core: MIDI DIN on USART6 --------------------------------------- */
#define MIDI_RX_PORT     GPIOG
#define MIDI_RX_PIN      9      /* AF8 */
#define MIDI_TX_PORT     GPIOG
#define MIDI_TX_PIN      14     /* AF8 */
#define MIDI_BAUD        31250u

/* ---- Core: micro-SD (SDIO), USB, SDRAM — later phases --------------- */
#define SD_DETECT_PORT   GPIOD
#define SD_DETECT_PIN    13     /* low = card present */
#define USBH_PWR_EN_PORT GPIOD
#define USBH_PWR_EN_PIN  7      /* ACTIVE LOW enable of the USB-A 5 V switch (measured 2026-09-18): low = port powered */
#define USBH_OC_PORT     GPIOG
#define USBH_OC_PIN      13     /* low = over-current */
#define VBUS_MON_PORT    GPIOF
#define VBUS_MON_PIN     10     /* ADC3_IN8 */

#endif
