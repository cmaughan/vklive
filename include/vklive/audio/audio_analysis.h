#pragma once

#include <atomic>
#include <vklive/audio/audio.h>
#include <concurrentqueue/concurrentqueue.h>

namespace Audio
{

bool audio_analysis_start(AudioAnalysis& analyis, const AudioChannelState& state);
void audio_analysis_stop(AudioAnalysis& analyis);

void audio_analysis_destroy(AudioAnalysis& analysis);
void audio_analysis_update(AudioAnalysis& analysis, AudioBundle& bundle);

} // namespace Audio
