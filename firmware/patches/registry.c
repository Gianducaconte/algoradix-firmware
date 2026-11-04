#include "patch.h"
extern const patch_t patch_st_bass, patch_st_lead, patch_st_obpad, patch_st_strings, patch_st_keys, patch_st_arp, patch_st_dark, patch_st_drone,
                     patch_strang1, patch_strang2, patch_strang3, patch_strang4,
                     patch_polyvoice, patch_fm, patch_string,
                     patch_wavefold, patch_formant, patch_modal, patch_wavetable, patch_sampler;
/* the last entry is the SAMPLER; the ALGO page shows it once per card preset */
const patch_t *const patch_registry[] = {
  &patch_st_bass, &patch_st_lead, &patch_st_obpad, &patch_st_strings, &patch_st_keys, &patch_st_arp, &patch_st_dark, &patch_st_drone,
  &patch_strang1, &patch_strang2, &patch_strang3, &patch_strang4,
  &patch_polyvoice, &patch_fm, &patch_string,
  &patch_wavefold, &patch_formant, &patch_modal, &patch_wavetable, &patch_sampler,
};
const int patch_count = 20;
const int patch_sampler_index = 19;
