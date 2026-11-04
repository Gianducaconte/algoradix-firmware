# Third-party components

Everything in this repository is © 2026 Roffa Logics under GPL-3.0 ([LICENSE](LICENSE)), except the components
below. They're vendored unmodified and keep their own licences, which are all compatible with GPL-3.0.

| Component | Path | Licence | Notes |
|---|---|---|---|
| Arm CMSIS-Core | `firmware/cmsis/` | Apache-2.0 ([LICENSE-CMSIS.txt](firmware/cmsis/LICENSE-CMSIS.txt)) | core headers for the Cortex-M4 |
| ST STM32F4 device header | `firmware/cmsis/` | Apache-2.0 ([LICENSE-ST.md](firmware/cmsis/LICENSE-ST.md)) | register definitions for the STM32F427 |
| FatFs R0.15 by ChaN | `firmware/fatfs/` | FatFs licence, BSD-style ([LICENSE.txt](firmware/fatfs/LICENSE.txt)) | FAT filesystem for the SD card. `ffconf.h` holds our configuration; the disk glue `firmware/hal/diskio_sd.c` is ours |
| TinyUSB | `firmware/tinyusb/` | MIT ([LICENSE](firmware/tinyusb/LICENSE)) | host stack, MIDI host class, MSC device class, Synopsys DWC2 port. `tusb_config.h` is ours. Version in [VERSION.txt](firmware/tinyusb/VERSION.txt) |
| Liberation Sans Bold | `tools/fonts/` | SIL Open Font License 1.1 ([NOTICE.txt](tools/fonts/NOTICE.txt)) | rasterised by `tools/make_ui_font.py` into the bitmap tables of `firmware/panel/font_ui.c` |

## Hardware documentation

The Axoloti Core v1.2 was designed by Johannes Taelman. Its schematic is published by the
[Axoloti project](https://github.com/axoloti/axoloti). The pin and peripheral facts in this repository were checked
against that schematic and then measured on the module. No Axoloti firmware code is used.
