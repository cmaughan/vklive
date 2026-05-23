#pragma once

#include <thread>
#include <zest/time/timer.h>
#include <zest/math/math.h>
#include <zest/math/math_utils.h>
#include <zest/settings/settings.h>

#include <zest/file/serializer.h>

#include "profiler_data.h"

namespace Zest
{

namespace Profiler
{

struct ProfileSettings
{
    uint32_t MaxThreads = 120;
    uint32_t MaxCallStack = 20;
    uint32_t MaxEntriesPerThread = 100000;
    uint32_t MaxFrames = 10000;
    uint32_t MaxRegions = 10000;
};

void SetProfileSettings(const ProfileSettings& settings);
void Init();
void UnDump(std::shared_ptr<ProfilerData>& profilerData);
void NewFrame();
void NameThread(const char* pszName);
void SetPaused(bool pause);
void BeginRegion();
void EndRegion();
void SetRegionLimit(uint64_t maxTimeNs);
void PushSectionBase(const char*, uint32_t, const char*, int);
void PopSection();
void ShowProfile();
void HideThread();
void Finish();

struct ProfileScope
{
    ProfileScope(const char* szSection, uint32_t color, const char* szFile, int line)
    {
        PushSectionBase(szSection, color, szFile, line);
    }
    ~ProfileScope()
    {
        PopSection();
    }
};

struct RegionScope
{
    RegionScope()
    {
        BeginRegion();
    }
    ~RegionScope()
    {
        EndRegion();
    }
};
#define PROFILE_COL_LOCK 0xFF0000FF

template <class _Mutex>
class profile_lock_guard { // class with destructor that unlocks a mutex
public:
    using mutex_type = _Mutex;

    explicit profile_lock_guard(_Mutex& _Mtx, const char* name = "Mutex", const char* szFile = nullptr, int line = 0) : _MyMutex(_Mtx) { // construct and lock
        PushSectionBase(name, PROFILE_COL_LOCK, szFile, line);
        _MyMutex.lock();
        PopSection();
    }

    profile_lock_guard(_Mutex& _Mtx, std::adopt_lock_t) : _MyMutex(_Mtx) {} // construct but don't lock

    ~profile_lock_guard() noexcept {
        _MyMutex.unlock();
    }

    profile_lock_guard(const profile_lock_guard&) = delete;
    profile_lock_guard& operator=(const profile_lock_guard&) = delete;

private:
    _Mutex& _MyMutex;
};

#define LOCK_GUARD(var, name) \
::Zest::Profiler::profile_lock_guard name##_lock(var, #name, __FILE__, __LINE__)

const glm::vec4& ColorFromName(const char* pszName, const uint32_t len);

} // namespace Profiler
} // namespace Zest

// PROFILE_SCOPE(MyNameWithoutQuotes)
#define PROFILE_SCOPE(name) \
static const uint32_t name##_color = Zest::ToPackedARGB(Zest::Profiler::ColorFromName(#name, uint32_t(strlen(#name)))); \
Zest::Profiler::ProfileScope name##_scope(#name, name##_color, __FILE__, __LINE__);

// PROFILE_SCOPE(char*, ImColor32 bit value)
#define PROFILE_SCOPE_STR(str, col) \
Zest::Profiler::ProfileScope name##_scope(str, col, __FILE__, __LINE__);

// Mark one extra region
#define PROFILE_REGION(name) \
Zest::Profiler::RegionScope name##_region;

// Give a thread a name.
#define PROFILE_NAME_THREAD(name) \
Zest::Profiler::NameThread(#name);

// Hide a thread.  Not sure this is tested or works....
#define PROFILE_HIDE_THREAD() \
Zest::Profiler::HideThread();



