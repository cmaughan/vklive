#pragma once

#include <vklive/file/toml_utils.h>
#include <vklive/logger/logger.h>

#undef ERROR

namespace Audio
{

struct AudioAnalysisSettings
{
    uint32_t frames = 4096;
    float blendFactor = 100.0f;
    bool blendFFT = true;
    bool logPartitions = true;
    bool filterFFT = true;
    bool normalizeAudio = false;
    bool removeFFTJitter = false;
    uint32_t spectrumBuckets = 100;
    glm::uvec4 spectrumFrequencies = glm::uvec4(100, 500, 3000, 10000);
    glm::vec4 spectrumGains = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float audioDecibelRange = 70.0f;
};

inline AudioAnalysisSettings audioanalysis_load_settings(const toml::table& settings)
{
    AudioAnalysisSettings analysisSettings;

    if (settings.empty())
        return analysisSettings;

    try
    {
        analysisSettings.frames = settings["frames"].value_or(analysisSettings.frames);
        analysisSettings.blendFactor = settings["blend_factor"].value_or(analysisSettings.blendFactor);
        analysisSettings.blendFFT = settings["blend_fft"].value_or(analysisSettings.blendFFT);
        analysisSettings.logPartitions = settings["log_partitions"].value_or(analysisSettings.logPartitions);
        analysisSettings.filterFFT = settings["filter_fft"].value_or(analysisSettings.filterFFT);
        analysisSettings.removeFFTJitter = settings["dejitter_fft"].value_or(analysisSettings.removeFFTJitter);
        analysisSettings.spectrumFrequencies = toml_read_vec4(settings["spectrum_frequencies"], analysisSettings.spectrumFrequencies);
        analysisSettings.spectrumGains = toml_read_vec4(settings["spectrum_gains"], analysisSettings.spectrumGains);
    }
    catch (std::exception& ex)
    {
        LOG(ERROR, ex.what());
    }
    return analysisSettings;
}

inline toml::table audioanalysis_save_settings(const AudioAnalysisSettings& settings)
{
    auto freq = settings.spectrumFrequencies;
    auto gain = settings.spectrumGains;

    auto tab = toml::table {
        { "frames", int(settings.frames) },
        { "blend_factor", settings.blendFactor },
        { "blend_fft", settings.blendFFT },
        { "filter_fft", settings.filterFFT },
        { "dejitter_fft", settings.removeFFTJitter },
        { "log_partitions", settings.logPartitions },
        { "spectrum_frequencies", toml::array{ freq.x, freq.y, freq.z, freq.w } },
        { "spectrum_gains", toml::array{ gain.x, gain.y, gain.z, gain.w } }
    };

    return tab;
}

} // namespace Audio
