/* POLY VOICE — 8-voice subtractive: morph osc + sub + noise -> voice frame (filter + env VCA). */
#include "poly_patch.h"
#include "voice.h"
static void vreset(voice_t *v, uint32_t seed) { voice_reset(v, 0x1234567u * seed); }
DEFINE_POLY_PATCH(polyvoice, "POLY VOICE", "SHAPE", "SUB/NOISE", voice_t, voice_ops, vreset, (void)0)
