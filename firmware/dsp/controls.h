/* The one struct the audio code reads. Written by the 1 kHz control task,
 * read once per block by the DSP. All continuous values are already filtered
 * and scaled; the DSP does no I/O and knows nothing about ADCs. */
#ifndef CONTROLS_H
#define CONTROLS_H
#include <stdint.h>

enum { ENV_AD = 0, ENV_ASR, ENV_LOOP, ENV_MODES };
enum { FILT_LP = 0, FILT_BP, FILT_HP, FILT_LADDER, FILT_TYPES };
enum { LFO_OFF = 0, LFO_PITCH, LFO_CUTOFF, LFO_PWM, LFO_DESTS };
enum { DSYNC_OFF = 0, DSYNC_16, DSYNC_8, DSYNC_8D, DSYNC_4, DSYNC_MODES };
enum { CVMODE_SINGLE = 0, CVMODE_UNISON, CVMODE_MODES };
enum { AIN_MUTE = 0, AIN_MIX, AIN_FILTER, AIN_RING, AIN_MODES };
enum { AINCH_STEREO = 0, AINCH_MONO, AINCH_MODES };
enum { PM_POLY = 0, PM_UNISON, PM_MONO, PM_MODES };                 /* MODE page */
enum { ENVD_VCA = 0, ENVD_VCA_FLT, ENVD_FILTER, ENVD_PITCH, ENVD_F1, ENVD_F2, ENVD_MODES };   /* what the A/D envelope drives */
enum { CVD_GATE = 0, CVD_OCT, CVD_ATTACK, CVD_DECAY, CVD_FILT, CVD_F1, CVD_F2, CVD_LEVEL, CVD_MODES };   /* what the ASSIGN jack drives */
enum { GATESRC_ALL = 0, GATESRC_CV, GATESRC_MIDI, GATESRC_MODES };   /* yellow button is a shift key, never a gate */

typedef struct {
  /* --- continuous, from panel (0..1 unless noted) --- */
  float macro1, macro2;
  float cutoff;          /* 0..1 -> 20 Hz .. 20 kHz exponential */
  float attack, decay;   /* 0..1 -> time, exponential mapping in env.c */
  float level;           /* master */
  float cv_note;         /* MIDI-note-like pitch for the CV voice: base + TUNE(pot+CV) + OCT */
  int   cv_gate;         /* gate for the CV voice (ASSIGN jack / button) */
  int   octave;          /* -2..+2 from OCT pot, applied to every voice */
  float tune_semis;      /* extra transpose applied to MIDI voices (0 unless "TUNE->MIDI" is on) */
  /* --- settings, from menu --- */
  uint8_t env_mode;      /* ENV_* */
  uint8_t filter_type;   /* FILT_* */
  float   resonance;     /* 0..1 */
  float   env_to_cutoff; /* 0..1 envelope amount towards env_dest (kept name: the filter was the only target before) */
  uint8_t env_dest;      /* ENVD_* */
  float   key_track;     /* 0..1 */
  uint8_t cv_mode;       /* CVMODE_* */
  uint8_t audio_in;      /* AIN_* */
  uint8_t audio_in_ch;   /* AINCH_* */
  uint8_t cv_dest;       /* CVD_* : what the ASSIGN jack controls */
  uint8_t midi_thru;     /* 0/1: DIN in (and USB in) echoed to DIN out */
  uint8_t voices;        /* 1..DSP_MAX_VOICES */
  uint8_t gate_src;      /* GATESRC_* */
  uint8_t midi_channel;  /* 0 = omni, 1..16 */
  uint8_t tune_to_midi;  /* 0/1 */
  uint8_t patch_index;
  /* --- effects (GATE + F1 = delay mix, GATE + F2 = reverb mix; rest in menu) --- */
  float dly_mix, rev_mix;          /* 0..1 sends */
  float dly_time, dly_feedback;    /* 0..1 */
  float rev_size, rev_damp;        /* 0..1 */
  float chorus;                    /* 0..1 chorus mix */
  float modwheel;                  /* 0..1 from MIDI CC1 (not saved) */
  int16_t  sample_sel;             /* index into the card's preset list, -1 = none (runtime) */
  uint32_t sample_hash;            /* hash of the selected preset's path, restored at boot */
  /* --- unison / LFO (menu) --- */
  uint8_t  unison;                 /* voices per MIDI note: 1..3 */
  float    uni_detune;             /* 0..1 -> 0..30 cents spread */
  float    lfo_rate;               /* 0..1 -> 0.05 .. 20 Hz */
  uint8_t  lfo_dest;               /* LFO_* */
  float    lfo_amt;                /* 0..1 */
  float    drive;                  /* 0..1 ladder drive */
  float    lfo_out;                /* runtime: -1..1, written by the control task */
  /* --- envelope / analog feel / glide --- */
  float    sustain;                /* 0..1 sustain level (ASR becomes ADSR) */
  float    drift;                  /* 0..1 per-voice analog drift amount */
  float    glide;                  /* 0..1 portamento time */
  uint8_t  dly_sync;               /* DSYNC_* : delay time follows the MIDI clock */
  float    fenv_attack, fenv_decay; /* 0..1 filter envelope (FUNC + A / D); 0,0 = off */
  uint8_t  play_mode;               /* PM_* */
} controls_t;

void controls_defaults(controls_t *c);
#endif
