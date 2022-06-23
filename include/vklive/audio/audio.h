#pragma once

#include <chrono>
#include <mutex>
#include <portaudio.h>
#include <complex>
#include <atomic>

#include <vklive/logger/logger.h>

#include <vklive/memory.h>
#include <vklive/audio/audio_analysis_settings.h>
#include <vklive/audio/audio_device_settings.h>

namespace mkiss
{
#include <kiss_fft.h>
}

union SDL_Event;

namespace Audio
{

struct AudioSettings
{
    std::atomic<bool> enableMetronome = false;
};

using AudioCB = std::function<void(const double beat, const double quantum, std::chrono::microseconds hostTime, void* pOutput, const void* pInput, uint32_t frameCount)>;

/*
struct DeviceInfo
{
    std::string fullName;
};
*/

struct ApiInfo
{
    int NumOutDevices() const
    {
        return int(outDeviceNames.size());
    }
    int NumInDevices() const
    {
        return int(inDeviceNames.size());
    }
    std::vector<uint32_t> outDeviceApiIndices;
    std::vector<std::string> outDeviceNames;
    std::map<uint32_t, std::vector<uint32_t>> outSampleRates;

    std::vector<uint32_t> inDeviceApiIndices;
    std::vector<std::string> inDeviceNames;
    std::map<uint32_t, std::vector<uint32_t>> inSampleRates;
};

struct AudioChannelState
{
    uint32_t frames = 0;
    int64_t totalFrames = 0;
    uint32_t channelCount = 2;
    uint32_t sampleRate = 44000;
    double deltaTime = 1.0f / (double)sampleRate;
};

struct AudioAnalysis
{
    // FFT
    mkiss::kiss_fft_cfg cfg;
    std::vector<std::complex<float>> fftIn;
    std::vector<std::complex<float>> fftOut;
    std::vector<float> fftMag;
    std::vector<float> window;
    bool fftConfigured = false;

    AudioChannelState channel;

    // Double buffer the data
    static const uint32_t SwapBuffers = 2;
    std::vector<float> spectrumBuckets[SwapBuffers];
    std::vector<float> spectrum[SwapBuffers];
    std::vector<float> audio[SwapBuffers];
    uint32_t triggerIndex[SwapBuffers] = { 0, 0 };
    uint32_t currentBuffer = 0;

    std::vector<float> frameCache;

    uint32_t outputSamples = 0; // The FFT output frames

    float totalWin = 0.0f;

    float currentMaxSpectrum;
    uint32_t maxSpectrumIndex;

    glm::vec4 spectrumBands = glm::vec4(0.0);

    const float LowHarmonic = 60.0f;
    const float MaxLowHarmonic = 80.0f;
    float lastPeakHarmonic = LowHarmonic;
    float lastPeakFrequency = 0.0f;

    bool audioActive = false;

    std::vector<float> spectrumPartitions;
    std::pair<uint32_t, uint32_t> lastSpectrumPartitions = { 0, 0 };
};

struct ChannelProcessResults
{
    std::vector<std::shared_ptr<AudioAnalysis>> activeChannels;
};

struct AudioContext
{
    bool m_initialized = false;
    bool m_isPlaying = true;

    AudioCB m_fnCallback = nullptr;

    AudioChannelState inputState;
    AudioChannelState outputState;
    

    AudioSettings settings;
    AudioDeviceSettings audioDeviceSettings;

    // Audio analysis information. Processed outside of audio thread, consumed in UI,
    // so use system mutex, we don't need to spin
    PNL_CL_Memory<ChannelProcessResults, std::mutex> audioInputAnalysis;
    PNL_CL_Memory<ChannelProcessResults, std::mutex> audioOutputAnalysis;
    AudioAnalysisSettings audioAnalysisSettings;

    uint64_t m_sampleTime;
    std::thread::id threadId;
    bool m_audioValid = false;
    bool m_changedDeviceCombo = true;
    std::vector<std::string> m_deviceNames;
    std::vector<std::string> m_apiNames;
    std::map<uint32_t, ApiInfo> m_mapApis;
    std::vector<std::string> m_currentRateNames;
    std::vector<uint32_t> m_currentRates;

    // PortAudio
    PaStreamParameters m_inputParams;
    PaStreamParameters m_outputParams;
    PaStream* m_pStream = nullptr;
};

AudioContext& GetAudioContext();

inline glm::uvec4 Div(const glm::uvec4& val, uint32_t div)
{
    return glm::uvec4(val.x / div, val.y / div, val.z / div, val.w / div);
}

bool audio_init(const AudioCB& fnCallback);
void audio_destroy();
void audio_show_gui();

} // namespace Audio
