#pragma once

#include <atomic>
#include <vklive/audio/audio.h>
#include <concurrentqueue/concurrentqueue.h>

namespace Audio
{

bool audio_analysis_start(AudioAnalysis& analyis, const AudioChannelState& state);
void audio_analysis_stop(AudioAnalysis& analyis);
bool audio_analysis_init(AudioAnalysis& analysis, const AudioChannelState& state);

void audio_analysis_destroy(AudioAnalysis& analysis);
void audio_analysis_update(AudioAnalysis& analysis, AudioBundle& bundle);
const std::vector<float>& audio_analysis_get_spectrum_buckets(AudioAnalysis& analysis);
const std::vector<float>& audio_analysis_get_audio(AudioAnalysis& analysis);

} // namespace Audio
