#pragma once

#include <vector>
#include <vklive/logger/logger.h>
#include <toml++/toml.h>

#undef ERROR

namespace Audio
{

struct AudioDeviceSettings
{
    bool enableInput = true;
    bool enableOutput = false;
    int apiIndex = 0;
    int inputDevice = -1;
    int outputDevice = -1;
    uint32_t inputChannels = 2;
    uint32_t outputChannels = 2;
    uint32_t sampleRate = 0;        // 0 is default / preferred rate
    uint32_t frames = 1024;          // default frames
};

/*
inline toml::table audiodevice_save_settings(const AudioDeviceSettings& settings)
{
    toml::table tab;
    tab["api"] = int(settings.apiIndex);
    tab["frames"] = int(settings.frames);
    tab["sample_rate"] = int(settings.sampleRate);

    tab["output_enable"] = bool(settings.enableOutput);
    tab["output_channels"] = int(settings.outputChannels);
    tab["output_device"] = int(settings.outputDevice);
    
    tab["input_enable"] = bool(settings.enableInput);
    tab["input_channels"] = int(settings.inputChannels);
    tab["input_device"] = int(settings.inputDevice);

    return tab;
}

inline AudioDeviceSettings audiodevice_load_settings(const toml::table& settings)
{
    AudioDeviceSettings deviceSettings;

    if (settings.empty())
        return deviceSettings;

    // TODO: Make string keys to save duplication
    try
    {
        deviceSettings.apiIndex = toml_get<int>(settings, "api", int(deviceSettings.apiIndex));
        deviceSettings.frames = toml_get<int>(settings, "frames", int(deviceSettings.frames));
        deviceSettings.sampleRate = toml_get<int>(settings, "sample_rate", int(deviceSettings.sampleRate));

        deviceSettings.enableOutput = toml_get<bool>(settings, "output_enable", deviceSettings.enableOutput);
        deviceSettings.outputChannels = toml_get<int>(settings, "output_channels", int(deviceSettings.outputChannels));
        deviceSettings.outputDevice = toml_get<int>(settings, "output_device", int(deviceSettings.outputDevice));

        deviceSettings.enableInput = toml_get<bool>(settings, "input_enable", deviceSettings.enableInput);
        deviceSettings.inputChannels = toml_get<int>(settings, "input_channels", int(deviceSettings.inputChannels));
        deviceSettings.inputDevice = toml_get<int>(settings, "input_device", int(deviceSettings.inputDevice));
    }
    catch (toml::exception & ex)
    {
        M_UNUSED(ex);
        LOG(ERROR, ex.what());
    }
    return deviceSettings;
}
*/

} // Audio
