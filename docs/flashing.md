# Flashing, backup and recovery

## Why you can't brick the module

The STM32F427 has a **ROM bootloader in system memory**. Nothing you flash can erase or change it. On the Axoloti
Core, **S1 is wired straight to BOOT0** (with a 47 k pull-down). If you hold S1 while power comes up, the chip starts
the ROM DFU loader, which shows up as `0483:df11` on the mini-USB port. From there `dfu-util` can rewrite the whole
application flash, however broken the firmware in it is.

You can only lose that escape hatch by:

1. Setting read-out protection level 2, or write-protecting sectors through the **option bytes**. This firmware never
   writes `FLASH->OPTCR`.
2. Physically damaging BOOT0/S1 or the USB port.

## Entering DFU mode

1. Unplug the mini-USB. (Eurorack power can stay on or off. The Core can also run from USB.)
2. **Hold S1** on the Core, plug in the mini-USB, then release S1.
3. `dfu-util -l` lists `Found DFU: [0483:df11]`.

On Windows, the ROM bootloader may need the WinUSB driver. Install it once with [Zadig](https://zadig.akeo.ie/).

## Backing up the original flash (do this once, first)

```bash
dfu-util -d 0483:df11 -a 0 -s 0x08000000:1048576 -U axoloti-core-original.bin
```

This reads the full 1 MB, firmware plus settings sector. Keep the file. Putting it back with `-D` (the command
below) restores the module exactly as it was. From the source tree, `make -C firmware backup` does the same and saves
the file with a date in its name.

## Flashing

```bash
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D releases/algoradix-v1.0.0.bin
```

or, after building, `make -C firmware flash`.

`:leave` makes the chip jump into the new firmware right away. The ROM bootloader only jumps if the image's initial
stack pointer is in SRAM (0x20000000…0x2001FFFF). This firmware's stack is at 0x2001FFF8, and its startup code forces
a clean system reset when it detects it was entered by a jump. (Axoloti firmware keeps its stack in CCM, so after
flashing one of those you have to power-cycle the module.)

Recovering from anything works the same way: DFU mode, then flash a known-good file.

## Settings

Settings and per-patch presets live in flash **sector 11** (0x080E0000, 128 KB) as an append-only log with CRC-32.
Flashing the firmware doesn't erase them. For a factory reset, write an erased (all 0xFF) sector over it in DFU mode:

```bash
python3 -c "import sys; sys.stdout.buffer.write(b'\xff' * 131072)" > blank-sector.bin
dfu-util -d 0483:df11 -a 0 -s 0x080E0000 -D blank-sector.bin
```

## Bring-up stages

`make STAGE=n` builds small test firmwares. They were used to bring up a new unit one block at a time and are useful
if you build your own hardware.

| Stage | What it does |
|---|---|
| 0 | heartbeat: the Core's green LED blinks once a second (fast red blinking = the 168 MHz clock didn't start). The encoder moves a lit ring LED, GATE lights all four, the encoder push lights red |
| 1 | panel test on the display: pot, CV, encoder and button values on three pages (S2 or encoder push = next page) |
| 2 | audio test: silence, a 440 Hz tone (gated by GATE, level on LEVELS) or audio in passed to audio out; codec status and DSP load on the display |
| 3 | the earlier synth with all 19 engines built in (no SD patches) |
| 4 | SD card test: mounts the card and lets you browse its folders |
| 5 | **the patch host** (default, the release) |

## Optional: SWD

JP1 on the Core has pads for an ST-Link (SWCLK PA14, SWDIO PA13, NRST, VDD, GND). You don't need it for flashing, but
it gives you a real debugger.

## Debug output

There's no free UART on the GPIO header. For debugging, use the Core LEDs (green PG6, red PC6), the panel LEDs, the
display (FUNC + hold the encoder for 1.5 s shows the statistics screen), and the desktop simulator in `sim/` for
anything DSP.
