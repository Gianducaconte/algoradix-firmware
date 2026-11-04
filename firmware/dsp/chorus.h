/* Stereo chorus (two modulated taps, Juno-style). Mono in from the mix, wet added to L/R. */
#ifndef CHORUS_H
#define CHORUS_H
#define CHORUS_LEN 2048
typedef struct { float buf[CHORUS_LEN]; int wr; float lfo; } chorus_t;
void chorus_init(chorus_t *c);
void chorus_process(chorus_t *c, float mix, float depth01, float *l, float *r, int n);
#endif
