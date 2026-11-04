# Hardware facts (measured)

This page is what the firmware relies on. The pins were checked against the Axoloti Core v1.2 schematic, and every
polarity and scaling below was **measured on the module**. Where measurement and upstream documentation disagreed,
measurement won (see the USB host power enable). The single source of truth in code is
[`firmware/board/board_algoradix.h`](../firmware/board/board_algoradix.h). The interface board circuits behind these
numbers are documented in the [hardware repository](https://github.com/Gianducaconte/algoradix-hardware/blob/main/docs/interface-board.md).

## Panel → MCU (Axoloti Core GPIO header)

The header exposes PA0–PA7, PB0, PB1, PB6–PB9, PC0–PC5, plus VDD, VDDA, +5 V and GND. Every one of those pins except
PB1 is used.

### Analog (ADC1, 12-bit, 0–3.3 V)

| Panel | MCU pin | ADC1 ch | CV jack |
|---|---|---|---|
| f1 | PA5 | 5 | yes |
| f2 | PA3 | 3 | yes |
| TUNE | PC4 | 14 | yes (1 V/oct) |
| OCT | PC1 | 11 | — |
| A | PC2 | 12 | — |
| D | PC3 | 13 | yes |
| FILT | PB0 | 8 | yes |
| LEVELS | PC0 | 10 | — |
| ASSIGN | PA7 | 7 | CV only, no pot |

The ADC scans continuously in the order `F1, F2, TUNE, OCT, A, D, FILT, LEVELS, ASSIGN`, through DMA2 stream 0
with 144-cycle sampling.

| Fact | Value |
|---|---|
| Pots | read **inverted**: fully left ≈ 4080, fully right 0. Firmware: `pot = POT_RAW_MAX - raw` |
| CV jacks | **inverting**, 400 ADC counts per volt, summed with the pot on the same pin (the interface board sums them with an op-amp) |
| ASSIGN | 0 V reads 2222 counts; a 5 V gate reads about 190. Gate on below 1800, off above 2000 (hysteresis) |
| Noise | about ±5 counts |

### Digital

| Panel | MCU pin | Notes |
|---|---|---|
| GATE button (yellow) | PB6 | **active high**, internal pull-down |
| Encoder push | PB7 | **active high**, internal pull-down |
| Encoder A / B | PC5 / PA2 | EC11, 20 detents, one full quadrature cycle per detent, pull-ups on the board |
| Ring LED 1 (ALGO, top left) | PA6 | active high, TIM3_CH1 capable |
| Ring LED 2 (CLOCK/VCA, top right) | PA0 | active high, TIM2_CH1 / TIM5_CH1 capable |
| Ring LED 3 (PATCH, bottom left) | PA1 | active high, TIM2_CH2 / TIM5_CH2 capable |
| Ring LED 4 (GATE, bottom right) | PA4 | active high, no timer: on/off or software PWM |
| OLED | PB8 SCL / PB9 SDA | I2C1, SSD1306 **64×32**, address 0x3C, RAM columns 32…95 |
| Spare | PB1 | ADC1_IN9 / TIM3_CH4, unused |

The faceplate window hides about 6 px on each side of the 64 px display. Keep text inside x = 8…56, or centre it.

## Axoloti Core peripherals

| Block | Configuration |
|---|---|
| MCU / clock | STM32F427IGH6, HSE 8 MHz → PLL 168 MHz (M 8, N 336, P 2, Q 7), APB1 42 MHz, APB2 84 MHz |
| Codec | ADAU1961. Control over I2C3 (PH7 SCL, PH8 SDA, address 0x38). MCLK = MCO1 on PA8 (8 MHz). SAI1 as slave: PE5 SCK, PE4 FS, PE6 SD_A (in), PE3 SD_B (out), 32-bit slots, no halfword swap |
| Audio DMA | DMA2 S1 ch0 (out), DMA2 S4 ch1 (in), double buffer, 16 frames per block at 48 kHz (3 kHz interrupt) |
| SDRAM | 8 MB at 0xC0000000, FMC bank 1, 16-bit, CAS 2, SDCLK = HCLK/2, self-test at boot. Delay lines and samples live here |
| micro-SD | SDIO 4-bit: PC8–PC11 D0–D3, PC12 CLK, PD2 CMD; card detect PD13 (low = card present) |
| MIDI DIN | USART6: PG9 RX, PG14 TX, 31250 baud 8N1. TX carries the thru |
| USB device (mini-USB) | OTG_FS, PA11 / PA12. Mass storage for the SD card; the ROM DFU bootloader also enumerates here |
| USB host (USB-A) | OTG_HS with the internal full-speed PHY, PB14 / PB15. Port power enable **PD7 is ACTIVE LOW** (measured: powered with PD7 low or floating, off with PD7 high, the opposite of the upstream docs). Over-current flag PG13 is read for diagnostics only |
| Core LEDs | green PG6, red PC6, active high |
| Core S1 | BOOT0 only (hold at power-up = ROM DFU). Not readable as GPIO |
| Core S2 | PA10, shared with the USB ID line: polarity depends on USB, so the firmware learns the idle level at boot |
| Settings flash | sector 11 (0x080E0000, 128 KB), append-only log of 256-byte records with CRC-32 |

## DMA map

| Stream | User |
|---|---|
| DMA2 S0 | ADC1 |
| DMA2 S1 | SAI1_A (audio out) |
| DMA2 S4 | SAI1_B (audio in) |
| DMA2 S6 | SDIO |

## Electrical rules the firmware keeps

- Never enable DAC1 or DAC2 (PA4 and PA5 are LED 4 and the f1 pot).
- Pot and CV pins are always analog; never drive them as outputs.
- Never touch the option bytes (read-out or write protection). This keeps the ROM DFU recovery always available.
- Flash writes stall code fetches for milliseconds, so settings are only saved while no note is sounding.
