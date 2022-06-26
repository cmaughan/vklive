#pragma once

#include <vklive/audio/audio.h>

namespace Audio
{
struct AudioAnalysis;

bool audio_analysis_init(AudioAnalysis& analyis, const AudioChannelState& state);
void audio_analysis_destroy(AudioAnalysis& analysis);
void audio_analysis_update(AudioAnalysis& analysis, const float* data, uint32_t num, uint32_t stride);
const std::vector<float>& audio_analysis_get_spectrum_buckets(AudioAnalysis& analysis);
const std::vector<float>& audio_analysis_get_audio(AudioAnalysis& analysis);

} // namespace Audio
