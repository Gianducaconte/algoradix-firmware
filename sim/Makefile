# Host build of the portable DSP core: renders test WAVs on the Mac.
CC     := clang
FW     := ../firmware
CFLAGS := -O2 -g -std=c11 -Wall -Wextra -Wno-unused-parameter -DSIM -I$(FW)/dsp -I$(FW)/patches -I$(FW) -I$(FW)/patchsdk
SRC    := $(wildcard $(FW)/dsp/*.c) $(filter-out %/pool.c,$(wildcard $(FW)/patches/*.c)) $(FW)/patches/pool.c $(FW)/sampler/axb.c main_sim.c
all: build/sim
build/sim: $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -lm -o $@
# one APX patch, natively: make apx PATCH="demo/bass.c"   or   PATCH="templates/instrument_template.c"   (APXSIM=build/<name> for parallel builds)
SDK := $(FW)/patchsdk
APXSIM ?= build/apxsim
apx:
	@mkdir -p build
	$(CC) $(CFLAGS) $(CFLAGS_EXTRA) -DAPX_PATCH -I$(SDK) $(wildcard $(FW)/dsp/*.c) $(FW)/patches/pool.c $(SDK)/apx_inst.c $(addprefix $(SDK)/,$(PATCH)) apx_sim.c -lm -o $(APXSIM)
# UI previews: the firmware's own screen-drawing code, rendered to build/ui/sheet.png
ui:
	@mkdir -p build/ui
	$(CC) -O2 -std=c11 -Wall -Wextra -I$(FW) -I$(FW)/panel $(FW)/panel/gfx.c $(FW)/panel/font_ui.c $(FW)/ui/screens.c ui_preview.c -o build/ui_preview
	./build/ui_preview && python3 ../tools/ui_preview.py build/ui
# animated UI: a scripted menu session -> build/ui/ui-navigation.mp4 and .gif (needs ffmpeg)
ui-video:
	@mkdir -p build/ui
	$(CC) -O2 -std=c11 -Wall -Wextra -I$(FW) -I$(FW)/panel $(FW)/panel/gfx.c $(FW)/panel/font_ui.c $(FW)/ui/screens.c ui_anim.c -o build/ui_anim
	./build/ui_anim && python3 ../tools/ui_video.py build/ui
run: build/sim
	./build/sim build/polyvoice.wav
clean:
	rm -rf build
