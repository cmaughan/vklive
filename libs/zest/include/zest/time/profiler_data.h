#pragma once

#include <zest/file/serializer.h>
namespace Zest
{

namespace Profiler
{

struct ProfilerEntry
{
    // static infos
    const char* szSection = nullptr;
    const char* szFile = nullptr;
    uint64_t oldSectionPointer;
    uint64_t oldFilePointer;
    int line;
    unsigned int color;
    // infos used for rendering infos
    int level = 0;
    int64_t startTime;
    int64_t endTime;
    uint32_t parent;
};

struct FrameThreadInfo
{
    uint32_t threadIndex;
    uint32_t activeEntry;
};

struct Region
{
    std::string name;
    int64_t startTime;
    int64_t endTime;
};

struct Frame : Region
{
    uint32_t frameThreadCount = 0;
    std::vector<FrameThreadInfo> frameThreads;
};

struct ThreadData
{
    bool initialized;
    uint32_t callStackDepth = 0;
    uint32_t maxLevel = 0;
    int64_t minTime;
    int64_t maxTime;
    uint32_t currentEntry = 0;
    bool hidden = false;
    std::string name;
    std::vector<ProfilerEntry> entries;
    std::vector<uint32_t> entryStack;
};

// Everything needed to display a profile and capture relevent info
struct ProfilerData
{
    std::vector<ThreadData> threadData;
    std::vector<Frame> frameData;
    std::vector<Region> regionData;
    int64_t maxFrameTime;
    uint32_t currentFrame;
    uint32_t currentRegion;
    int64_t regionTimeLimit;
    std::vector<uint64_t> stringPointers;
    std::vector<std::string> strings;
};

inline void serialize(binary_writer& w, const ProfilerEntry& t)
{
    // static infos
    serialize(w, (uint64_t)t.szSection);
    serialize(w, (uint64_t)t.szFile);
    serialize(w, t.line);
    serialize(w, t.color);
    serialize(w, t.level);
    serialize(w, t.startTime);
    serialize(w, t.endTime);
    serialize(w, t.parent);
}

inline void deserialize(binary_reader& r, ProfilerEntry& t)
{
    // static infos
    deserialize(r, t.oldSectionPointer);
    deserialize(r, t.oldFilePointer);
    deserialize(r, t.line);
    deserialize(r, t.color);
    deserialize(r, t.level);
    deserialize(r, t.startTime);
    deserialize(r, t.endTime);
    deserialize(r, t.parent);
}

inline void serialize(binary_writer& w, const ProfilerData& t)
{
    serialize(w, t.threadData);
    serialize(w, t.frameData);
    serialize(w, t.regionData);
    serialize(w, t.maxFrameTime);
    serialize(w, t.currentFrame);
    serialize(w, t.currentRegion);
    serialize(w, t.regionTimeLimit);
    serialize(w, t.stringPointers);
    serialize(w, t.strings);
}

inline void deserialize(binary_reader& r, ProfilerData& t)
{
    deserialize(r, t.threadData);
    deserialize(r, t.frameData);
    deserialize(r, t.regionData);
    deserialize(r, t.maxFrameTime);
    deserialize(r, t.currentFrame);
    deserialize(r, t.currentRegion);
    deserialize(r, t.regionTimeLimit);
    deserialize(r, t.stringPointers);
    deserialize(r, t.strings);

}

inline void serialize(binary_writer& w, const ThreadData& t)
{
    serialize(w, t.initialized);
    serialize(w, t.callStackDepth);
    serialize(w, t.maxLevel);
    serialize(w, t.minTime);
    serialize(w, t.maxTime);
    serialize(w, t.currentEntry);
    serialize(w, t.hidden);
    serialize(w, t.name);
    serialize(w, t.entries);
    serialize(w, t.entryStack);
}

inline void deserialize(binary_reader& r, ThreadData& t)
{
    deserialize(r, t.initialized);
    deserialize(r, t.callStackDepth);
    deserialize(r, t.maxLevel);
    deserialize(r, t.minTime);
    deserialize(r, t.maxTime);
    deserialize(r, t.currentEntry);
    deserialize(r, t.hidden);
    deserialize(r, t.name);
    deserialize(r, t.entries);
    deserialize(r, t.entryStack);
}

inline void serialize(binary_writer& w, const Region& t)
{
    serialize(w, t.name);
    serialize(w, t.startTime);
    serialize(w, t.endTime);
}

inline void deserialize(binary_reader& r, Region& t)
{
    deserialize(r, t.name);
    deserialize(r, t.startTime);
    deserialize(r, t.endTime);
}

inline void serialize(binary_writer& w, const Frame& t)
{
    serialize(w, t.name);
    serialize(w, t.startTime);
    serialize(w, t.endTime);
    serialize(w, t.frameThreadCount);
    serialize(w, t.frameThreads);
}

inline void deserialize(binary_reader& r, Frame& t)
{
    deserialize(r, t.name);
    deserialize(r, t.startTime);
    deserialize(r, t.endTime);
    deserialize(r, t.frameThreadCount);
    deserialize(r, t.frameThreads);
}

} // namespace Profiler
} // namespace Zest     
