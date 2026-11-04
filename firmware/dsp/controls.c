#include "controls.h"
#include "dsp_config.h"
void controls_defaults(controls_t *c)
{
  c->macro1 = 0.3f; c->macro2 = 0.0f; c->cutoff = 0.7f; c->attack = 0.1f; c->decay = 0.4f;
  c->level = 0.8f; c->cv_note = 48.0f; c->cv_gate = 0; c->octave = 0; c->tune_semis = 0.0f;
  c->env_mode = ENV_ASR; c->filter_type = FILT_LP; c->resonance = 0.4f; c->env_to_cutoff = 0.0f;
  c->key_track = 0.5f; c->cv_mode = CVMODE_SINGLE; c->audio_in = AIN_MUTE; c->audio_in_ch = AINCH_STEREO; c->env_dest = ENVD_VCA; c->cv_dest = CVD_OCT; c->midi_thru = 1; c->voices = 6;
  c->gate_src = GATESRC_ALL; c->midi_channel = 0; c->tune_to_midi = 0; c->patch_index = 0;
  c->dly_mix = 0.0f; c->rev_mix = 0.0f; c->dly_time = 0.55f; c->dly_feedback = 0.4f; c->rev_size = 0.6f; c->rev_damp = 0.4f;
  c->chorus = 0.0f; c->modwheel = 0.0f; c->sample_sel = -1; c->sample_hash = 0;
  c->unison = 1; c->uni_detune = 0.4f; c->lfo_rate = 0.785f; c->lfo_dest = LFO_PITCH; c->lfo_amt = 0.0f;   /* 5.5 Hz vibrato waiting for the mod wheel */ c->drive = 0.2f; c->lfo_out = 0.0f;
  c->sustain = 1.0f; c->drift = 0.25f; c->glide = 0.0f; c->dly_sync = DSYNC_OFF; c->fenv_attack = 0.0f; c->fenv_decay = 0.0f; c->play_mode = PM_POLY;
}
