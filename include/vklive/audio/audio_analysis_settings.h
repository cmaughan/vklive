#pragma once

#include <glm/glm.hpp>
#include <vklive/logger/logger.h>
#include <toml++/toml.h>

#undef ERROR

namespace Audio
{

struct AudioAnalysisSettings
{
    uint32_t frames = 4096;
    float blendFactor = 10.0f;
    bool blendAudio = false;
    bool blendFFT = true;
    bool filterFFT = true;
    bool normalizeAudio = false;
    bool removeFFTJitter = false;
    uint32_t spectrumBuckets = 100;
    glm::uvec4 spectrumFrequencies = glm::uvec4(100, 500, 3000, 10000);
    glm::vec4 spectrumGains = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float audioDecibelRange = 70.0f;
};

/*
inline AudioAnalysisSettings audioanalysis_load_settings(const toml::table& settings)
{
    AudioAnalysisSettings analysisSettings;
    
    if (settings.empty())
        return analysisSettings;

    try
    {
        analysisSettings.frames = toml_get<int>(settings, "frames", int(analysisSettings.frames));
        analysisSettings.blendFactor = toml_get<float>(settings, "blend_factor", analysisSettings.blendFactor);
        analysisSettings.blendAudio = toml_get<bool>(settings, "blend_audio", analysisSettings.blendAudio);
        analysisSettings.blendFFT = toml_get<bool>(settings, "blend_fft", analysisSettings.blendFFT);
        analysisSettings.filterFFT = toml_get<bool>(settings, "filter_fft", analysisSettings.filterFFT);
        analysisSettings.removeFFTJitter = toml_get<bool>(settings, "dejitter_fft", analysisSettings.removeFFTJitter);
        analysisSettings.spectrumFrequencies = toml_get_vec4i(settings, "spectrum_frequencies", analysisSettings.spectrumFrequencies);
        analysisSettings.spectrumGains = toml_get_vec4(settings, "spectrum_gains", analysisSettings.spectrumGains);
    }
    catch (toml::exception & ex)
    {
        M_UNUSED(ex);
        LOG(ERROR, ex.what());
    }
    return analysisSettings;
}

inline toml::table audioanalysis_save_settings(const AudioAnalysisSettings& settings)
{
    toml::table tab;
    tab["frames"] = int(settings.frames);
    tab["blend_factor"] = settings.blendFactor;
    tab["blend_audio"] =settings.blendAudio;
    tab["blend_fft"] = settings.blendFFT;
    tab["filter_fft"] = settings.filterFFT;
    tab["dejitter_fft"] = settings.removeFFTJitter;

    auto freq = settings.spectrumFrequencies;
    auto gain = settings.spectrumGains;
    tab["spectrum_frequencies"] = { freq.x, freq.y, freq.z, freq.w };
    tab["spectrum_gains"] = { gain.x, gain.y, gain.z, gain.w };

    return tab;
}
*/

} // Audio
