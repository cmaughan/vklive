#include <algorithm>
#include <complex>
#include <cstdint>
#include <vector>

#include <glm/gtc/constants.hpp>

#include <vklive/audio/audio_analysis.h>
#include <vklive/audio/audio_analysis_settings.h>
#include <vklive/logger/logger.h>

#include <vklive/audio/audio.h>

namespace Audio
{

void audio_analysis_update(AudioAnalysis& analysis, const float* data, uint32_t num);
void audio_analysis_gen_linear_space(AudioAnalysis& analysis, uint32_t limit, uint32_t n);
void audio_analysis_gen_log_space(AudioAnalysis& analysis, uint32_t limit, uint32_t n);
// void audio_analysis_process();
void audio_analysis_calculate_spectrum(AudioAnalysis& analysis);
void audio_analysis_calculate_spectrum_bands(AudioAnalysis& analysis);
void audio_analysis_calculate_audio(AudioAnalysis& analysis);

namespace
{
/// Creates a Hamming Window for FFT
///
/// FFT requires a window function to get smooth results
inline std::vector<float> audio_analysis_create_window(uint32_t size)
{
    std::vector<float> ret(size);
    for (uint32_t i = 0; i < size; i++)
    {
        ret[i] = (0.5f * (1 - cos(2.0f * glm::pi<float>() * (i / (float)(size - 1)))));
    }

    return ret;
}
} // namespace

const uint32_t audio_analysis_get_width(AudioAnalysis& analysis)
{
    return uint32_t(analysis.outputSamples);
}

const std::vector<float>& audio_analysis_get_spectrum(AudioAnalysis& analysis)
{
    return analysis.spectrum[analysis.currentBuffer];
}

const std::vector<float>& audio_analysis_get_spectrum_buckets(AudioAnalysis& analysis)
{
    return analysis.spectrumBuckets[analysis.currentBuffer];
}

const std::vector<float>& audio_analysis_get_audio(AudioAnalysis& analysis)
{
    return analysis.audio[analysis.currentBuffer];
}

void audio_analysis_clear(AudioAnalysis& analysis)
{
    analysis.audioActive = false;
}

void audio_analysis_destroy(AudioAnalysis& analysis)
{
    if (analysis.cfg)
    {
        free(analysis.cfg);
        analysis.cfg = nullptr;
    }
}

bool audio_analysis_init(AudioAnalysis& analysis, const AudioChannelState& state)
{
    auto& ctx = GetAudioContext();
    analysis.outputSamples = (ctx.audioAnalysisSettings.frames / 2);

    analysis.channel = state;

    // Hamming window
    analysis.window = audio_analysis_create_window(ctx.audioAnalysisSettings.frames);
    analysis.totalWin = 0.0f;
    for (auto& win : analysis.window)
    {
        analysis.totalWin += win;
    }

    // Imaginary part of audio input always 0.
    analysis.fftIn.resize(ctx.audioAnalysisSettings.frames, std::complex<float>{ 0.0, 0.0 });
    analysis.fftOut.resize(ctx.audioAnalysisSettings.frames);
    analysis.fftMag.resize(ctx.audioAnalysisSettings.frames);

    for (uint32_t buf = 0; buf < analysis.SwapBuffers; buf++)
    {
        analysis.spectrum[buf].resize(analysis.outputSamples, (0));
        analysis.audio[buf].resize(ctx.audioAnalysisSettings.frames, 0.0f);
    }

    analysis.cfg = mkiss::kiss_fft_alloc(ctx.audioAnalysisSettings.frames, 0, 0, 0);

    return true;
}

void audio_analysis_update(AudioAnalysis& analysis, const float* data, uint32_t num, uint32_t stride)
{
    // PROFILE_SCOPE(Audio_Analysis);
    auto& ctx = GetAudioContext();

    auto frameOffset = 0; // ctx.audioAnalysisSettings.removeFFTJitter ? (uint32_t)-_lastPeakHarmonic & ~0x1 : 0;

    // Copy the data into the real part, windowing it to remove the transitions at the edges of the transform.
    // his is because the FF behaves as if your sample repeats forever, and would therefore generate extra
    // frequencies if the samples didn't perfectly tile (as they won't).
    // he windowing function smooths the outer edges to remove this transition and give more accurate results.

    analysis.currentBuffer = 1 - analysis.currentBuffer;
    auto& audioBuffer = analysis.audio[analysis.currentBuffer];

#ifdef _DEBUG
    for (size_t i = 0; i < num; i++)
    {
        assert(std::isfinite(data[i * stride]));
    }
#endif

    auto floatsToAdd = (num / stride);
    auto floatsToMove = audioBuffer.size() - floatsToAdd;
 
    if (floatsToAdd > 0)
    {
        const float* pSource;
        float* pDest;
        if (floatsToMove > 0)
        {
            pSource = (const float*)&audioBuffer[floatsToAdd];
            pDest = (float*)&audioBuffer[0];

            memmove(pDest, pSource, floatsToMove * sizeof(float));
        }

        pDest = (float*)&audioBuffer[floatsToMove];
        pSource = data;

        // Copy with stride
        for (uint32_t count = 0; count < floatsToAdd; count++)
        {
            *pDest++ = *pSource;
            pSource += stride;
        }
    }

    audio_analysis_calculate_audio(analysis);

    // Some of this math found here:
    //   https://github.com/beautypi/shadertoy-iOS-v2/blob/master/shadertoy/SoundStreamHelper.m
    if (analysis.audioActive)
    {
        for (uint32_t i = 0; i < ctx.audioAnalysisSettings.frames; i++)
        {
            // Hamming window, FF
            analysis.fftIn[i] = std::complex(audioBuffer[i] * analysis.window[i], 0.0f);
            // assert(std::isfinite(analysis.fftIn[i].real()));
        }

        mkiss::kiss_fft(analysis.cfg, (const mkiss::kiss_fft_cpx*)&analysis.fftIn[0], (mkiss::kiss_fft_cpx*)&analysis.fftOut[0]);

        // 0 for imaginary part
        analysis.fftOut[0] = std::complex(analysis.fftOut[0].real(), 0.0f);

        float scale = 1.0f / (ctx.audioAnalysisSettings.frames * 2);

        for (uint32_t i = 1; i < analysis.outputSamples; i++)
        {
            analysis.fftOut[i] = analysis.fftOut[i] * scale;
        }
        analysis.fftOut[0] = std::complex(analysis.fftOut[0].real(), 0.0f);

        // Convert to dB
        for (uint32_t i = 1; i < analysis.outputSamples; i++)
        {
            analysis.fftMag[i] = std::norm(analysis.fftOut[i]);
            analysis.fftMag[i] = 10 * std::log10(analysis.fftMag[i]);

            // Min decibels -100
            // Max decibels -30
            // Range is -128 -> 0 so adjust
            analysis.fftMag[i] += 74.0f;
            analysis.fftMag[i] *= (1.0f / 74.0f);

            // Clamp 0->1
            analysis.fftMag[i] = std::max(analysis.fftMag[i], 0.0f);
            analysis.fftMag[i] = std::min(analysis.fftMag[i], 1.0f);
        }

        audio_analysis_calculate_spectrum(analysis);
    }
}

void audio_analysis_calculate_audio(AudioAnalysis& analysis)
{
    // PROFILE_SCOPE(Analysis_Audio);
    auto& ctx = GetAudioContext();

    // TODO: This can't be right?
    auto samplesPerSecond = analysis.channel.sampleRate / (float)analysis.channel.frames;
    auto blendFactor = 64.0f / samplesPerSecond;

    // Copy the data into the real part, windowing it to remove the transitions at the edges of the transform.
    // his is because the FF behaves as if your sample repeats forever, and would therefore generate extra
    // frequencies if the samples didn't perfectly tile (as they won't).
    // he windowing function smooths the outer edges to remove this transition and give more accurate results.
    auto& audioBuf = analysis.audio[analysis.currentBuffer];
    auto& audioBufOld = analysis.audio[1 - analysis.currentBuffer];

    analysis.audioActive = false;

    // Find the min/max
    auto maxAudio = -std::numeric_limits<float>::max();
    auto minAudio = std::numeric_limits<float>::max();
    for (uint32_t i = 0; i < audioBuf.size(); i++)
    {
        auto fVal = audioBuf[i];
        maxAudio = std::max(maxAudio, fVal);
        minAudio = std::min(minAudio, fVal);
    }

    // Only process active audio
    if ((maxAudio - minAudio) > 0.001f)
    {
        analysis.audioActive = true;
    }

    // Normalize
    for (uint32_t i = 0; i < audioBuf.size(); i++)
    {
        if (ctx.audioAnalysisSettings.blendAudio)
        {
            // Scale and blend the audio
            audioBuf[i] = audioBuf[i] * blendFactor + audioBufOld[i] * (1.0f - blendFactor);
        }
        else if (ctx.audioAnalysisSettings.normalizeAudio)
        {
            audioBuf[i] = (audioBuf[i] - minAudio) / (maxAudio - minAudio);
        }
    }
}

void audio_analysis_calculate_spectrum(AudioAnalysis& analysis)
{
    // PROFILE_SCOPE(CalculateSpectrum);
    auto& ctx = GetAudioContext();

    float minSpec = std::numeric_limits<float>::max();
    float maxSpec = std::numeric_limits<float>::min();

    float maxSpectrumValue = std::numeric_limits<float>::min();
    uint32_t maxSpectrumBucket = 0;

    auto& spectrum = analysis.spectrum[analysis.currentBuffer];
    auto& spectrumOld = analysis.spectrum[1 - analysis.currentBuffer];
    auto& spectrumBuckets = analysis.spectrumBuckets[analysis.currentBuffer];

    for (uint32_t i = 0; i < analysis.outputSamples; i++)
    {
        // Magnitude
        const float ref = 1.0f; // Source reference value, but we are +/1.0f

        // Magnitude * 2 because we are half the spectrum,
        // divided by the total of the hamming window to compenstate
        //spectrum[i] = (analysis.fftMag[i] * 2.0f) / analysis.totalWin;
        //spectrum[i] = std::max(spectrum[i], std::numeric_limits<float>::min());
        spectrum[i] = analysis.fftMag[i];

        // assert(std::isfinite(spectrum[i]));

        if (spectrum[i] > maxSpectrumValue)
        {
            maxSpectrumValue = spectrum[i];
            maxSpectrumBucket = i;
        }

        // Log based on a reference value of 1
        //spectrum[i] = 20 * std::log10(spectrum[i] / ref);

        // Normalize by moving up and dividing
        // Decibels are now positive from 0->1;
        //spectrum[i] /= ctx.audioAnalysisSettings.audioDecibelRange;
        //spectrum[i] += 1.0f; // ctx.audioAnalysisSettings.audioDecibelRange;
        spectrum[i] = std::clamp(spectrum[i], 0.0f, 1.0f);

        if (i != 0)
        {
            minSpec = std::min(minSpec, spectrum[i]);
            maxSpec = std::max(maxSpec, spectrum[i]);
        }
    }

    // Convolve
    if (ctx.audioAnalysisSettings.filterFFT)
    {
        const int width = 4;
        for (int i = width; i < (int)analysis.outputSamples - width; ++i)
        {
            auto& spectrum = analysis.spectrum[analysis.currentBuffer];

            float total = (0);
            const float weight_sum = width * (2.0) + (0.5);
            for (int j = -width; j <= width; ++j)
            {
                total += ((1) - std::abs(j) / ((2) * width)) * spectrum[i + j];
            }
            spectrum[i] = (total / weight_sum);

            // assert(std::isfinite(spectrum[i]));
        }
    }

    if (ctx.audioAnalysisSettings.blendFFT)
    {
        // Time in ms / blend duration in ms
        auto samplesPerSecond = analysis.channel.sampleRate / (float)analysis.channel.frames;
        auto blendFactor = 64.0f / samplesPerSecond;
        for (uint32_t i = 0; i < analysis.outputSamples; i++)
        {
            // Blend with previous result
            spectrum[i] = spectrum[i] * blendFactor + spectrumOld[i] * (1.0f - blendFactor);
        }
    }

    {
        // Make less buckets on a big window, but at least 8
        uint32_t buckets = std::min(analysis.outputSamples / 8, uint32_t(ctx.audioAnalysisSettings.spectrumBuckets));
        buckets = std::max(buckets, uint32_t(4));
        buckets = 512;

        // Quantize into bigger buckets; filtering helps smooth the graph, and gives a more pleasant effect
        uint32_t spectrumSamples = uint32_t(spectrum.size());

        // Linear space shows lower frequencies, log space shows all freqencies but focused
        // on the lower buckets more
#define LINEAR_SPACE
#ifdef LINEAR_SPACE
        audio_analysis_gen_linear_space(analysis, spectrumSamples, buckets);
#else
        audio_analysis_gen_log_space(analysis, spectrumSamples, buckets);
#endif
        auto itrPartition = analysis.spectrumPartitions.begin();

        if (buckets > 0)
        {
            float countPerBucket = (float)spectrumSamples / (float)buckets;
            uint32_t currentBucket = 0;

            float av = 0.0f;
            uint32_t averageCount = 0;

            spectrumBuckets.resize(buckets);

            // Ignore the first spectrum sample
            for (uint32_t i = 1; i < spectrumSamples; i++)
            {
                av += spectrum[i];
                averageCount++;

                if (i >= *itrPartition)
                {
                    spectrumBuckets[currentBucket++] = av / (float)averageCount;
                    av = 0.0f; // reset sum for next average
                    averageCount = 0;
                    itrPartition++;
                }

                // Sanity
                if (itrPartition == analysis.spectrumPartitions.end()
                    || currentBucket >= buckets)
                    break;
            }
        }
    }
    
    audio_analysis_calculate_spectrum_bands(analysis);
}

// Divide the frequency spectrum into 4 values representing the average spectrum magnitude for
// each value's frequency range, as requested by the user.
// For example, vec4.x might end up containing 0->500Hz, vec4.y might be 500-1000Hz, etc.
void audio_analysis_calculate_spectrum_bands(AudioAnalysis& analysis)
{
    // PROFILE_SCOPE(Analysis_Bands);
    auto& ctx = GetAudioContext();

    auto samplesPerSecond = analysis.channel.sampleRate / (float)analysis.channel.frames;
    auto blendFactor = 64.0f / samplesPerSecond;
    auto bands = glm::vec4(0.0f);

    // Calculate the bucket offsets for each of the frequency bands in teh course partitions.
    auto frequencyPerBucket = float(analysis.channel.sampleRate) / float(analysis.channel.frames);
    auto spectrumOffsets = Div(ctx.audioAnalysisSettings.spectrumFrequencies, (int)frequencyPerBucket) + glm::uvec4(1, 1, 1, 0);

    for (uint32_t i = 0; i < analysis.outputSamples; i++)
    {
        for (uint32_t index = 0; index < 4; index++)
        {
            if (i < uint32_t(spectrumOffsets[index]))
            {
                bands[index] += analysis.spectrum[analysis.currentBuffer][i];

                break;
            }
        }
    }

    // Divide out by the size of the buckets sampled so that each band is evenly weighted
    float lastOffset = 0;
    for (uint32_t index = 0; index < 4; index++)
    {
        bands[index] /= float(spectrumOffsets[index] - lastOffset);
        lastOffset = float(spectrumOffsets[index]);
    }

    // Adjust by requested gain
    bands = bands * ctx.audioAnalysisSettings.spectrumGains;

    // Blend for smoother result
    analysis.spectrumBands = bands * blendFactor + analysis.spectrumBands * (1.0f - blendFactor);
}

// Generate a sequentially increasing space of numbers.
// The idea here is to generate partitions of the frequency spectrum that cover the whole
// range of values, but concentrate the results on the 'interesting' frequencies at the bottom end
// This code ported from the python below:
// https://stackoverflow.com/questions/12418234/logarithmically-spaced-integers
void audio_analysis_gen_log_space(AudioAnalysis& analysis, uint32_t limit, uint32_t n)
{
    auto& ctx = GetAudioContext();
    if (analysis.lastSpectrumPartitions == std::make_pair(limit, n) && !analysis.spectrumPartitions.empty())
    {
        return;
    }

    // Remember what we did last
    analysis.lastSpectrumPartitions = std::make_pair(limit, n);

    analysis.spectrumPartitions.clear();

    // Geneate buckets using a power factor, with each bucket advancing on the last
    uint32_t lastValue = 0;
    for (float fVal = 0.0f; fVal <= 1.0f; fVal += 1.0f / float(n))
    {
        const float curveSharpness = 4.0f;
        auto step = uint32_t(limit * std::pow(fVal, curveSharpness));
        step = std::max(step, lastValue + 1);
        lastValue = step;
        analysis.spectrumPartitions.push_back(float(step));
    }
}

// This is a simple linear partitioning of the frequencies
void audio_analysis_gen_linear_space(AudioAnalysis& analysis, uint32_t limit, uint32_t n)
{
    auto& ctx = GetAudioContext();
    if (analysis.lastSpectrumPartitions == std::make_pair(limit, n) && !analysis.spectrumPartitions.empty())
    {
        return;
    }

    // Remember what we did last
    analysis.lastSpectrumPartitions = std::make_pair(limit, n);

    analysis.spectrumPartitions.resize(n);
    for (uint32_t i = 0; i < n; i++)
    {
        analysis.spectrumPartitions[i] = (float(n) / (float(limit)));
    }
}

} // namespace Audio

/*
* From ShaderToy
realSignal[1024] //mono signal floats for input
imagSignal[1024] //can be all zeroes for input
spectumMag[512] //populate after FFT
fft.forward(realSignal,imagSignal)
spectrumLength = realSignal.length / 2
for (int i = 0; i <= spectrum.Length - 1; i++) {
	float magnitude = log10(sqrt(pow(realSignal[i], 2) + pow(imagSignal[i], 2)));
	spectumMag[i]= magnitude;
}
//normalize spectumMag[] between 0 to 1 using min and max values from spectumMag[]
min = spectumMag[].MinValue;
max = spectumMag[].MaxValue;
for (int i = 0; i <= spectrumMag.length - 1; i++) {
	spectrumMag[i] = (spectrumMag[i] - min) / (max - min);
    */
