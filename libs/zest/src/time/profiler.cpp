#include <algorithm>
#include <atomic>
#include <cassert>
#include <utility>

#include <zest/math/imgui_glm.h>
#include <zest/math/math_utils.h>
#include <zest/string/murmur_hash.h>
#include <zest/ui/dpi.h>

#include <zest/time/profiler.h>

#include "imgui_internal.h"
#include <format>

using namespace std::chrono;
using namespace Zest;

// Initial prototypes used:
// https://gist.github.com/CedricGuillemet/fc82e7a9b2f703ff1ebf
// https://github.com/arnaud-jamin/imgui/blob/master/profiler.cpp
//
// This one is better at spans that cover frame boundaries, uses cross platform cpp libraries
// instead of OS specific ones.
// No claims made about performance, but in practice has been very useful in finding bugs
// Has a summary view and support for zoom/pan, CTRL+select range or click in the summary graphs
// There is an optional single 'Region' which I use for audio frame profiling; you can only have one
// The profile macros can pick a unique/nice color for a given name.
// There is a LOCK_GUARD wrapper around a mutex, for tracking lock times
// Requires some helper code from my zest library (https://github.com/Rezonality/Zest) for:
// - NVec/NRect
// - murmur hash (for text to color)
// - dpi
// - fmt for text formatting
// I typically use this as a 'one shot, collect frames' debugger.  I hit the Pause/Resume button at interesting spots and collect a bunch of frames.
// You have to pause to navigate/inspect.
// All memory allocation is up-front; change the 'Max' values below to collect more frames
// The profiler just 'stops' when the memory is full.  It can be restarted/stopped.
// I pulled this together over the space of a weekend, it could be tidier here and there, but it works great ;)
namespace Zest
{

namespace Profiler
{

ProfileSettings settings;

DECLARE_SETTING_VALUE(c_AccentColor1)
DECLARE_SETTING_VALUE(c_AccentColor2);
DECLARE_SETTING_VALUE(c_Warning);
DECLARE_SETTING_VALUE(c_Error);

namespace
{

const unsigned int frameMarkerColor = 0x22FFFFFF;
const uint32_t MinLeadInFrames = 3;
const uint32_t MinFrame = MinLeadInFrames - 2;
const uint32_t MinSizeForTextDisplay = 5;

Zest::timer gTimer;
bool gPaused = true;
bool gRequestPause = false;
bool gRestarting = true;
bool gHideUI = false;

std::mutex gMutex;

std::atomic<uint64_t> gProfilerGeneration = 0;
thread_local int gThreadIndexTLS = -1;
thread_local uint64_t gGenerationTLS = -1;
float gMaxThreadNameSize = 0;

std::shared_ptr<ProfilerData> gProfilerData;
int32_t gSelectedThread = -1;

// Region
int64_t gRegionDisplayStart = 0;
int64_t gFrameDisplayStart = 0;
NRectf gCandleDragRect;

// Visible frame candle range
glm::vec2 gFrameCandleRange = glm::vec2(0.0f, 0.0f);

// Visible time range
glm::i64vec2 gTimeRange = glm::i64vec2(0, 0);

// Frames visible inside the current time range
glm::ivec2 gVisibleFrames = glm::ivec2(0, 0);

std::vector<glm::vec4> DefaultColors;

} // namespace

void Reset();

// Optionally call this before doing any profiler calls to change the defaults
void SetProfileSettings(const ProfileSettings& s)
{
    settings = s;
    Reset();
}

#define NUM_DEFAULT_COLORS 16

void CalculateColors()
{
    double golden_ratio_conjugate = 0.618033988749895;
    double h = .85f;
    for (int i = 0; i < (int)NUM_DEFAULT_COLORS; i++)
    {
        h += golden_ratio_conjugate;
        h = std::fmod(h, 1.0);
        DefaultColors.emplace_back(HSVToRGB(float(h) * 360.0f, 0.6f, 200.0f));
    }
}

// Run Init every time a profile is started
void Init()
{
    CalculateColors();

    gProfilerData = std::make_shared<ProfilerData>();
    gProfilerData->threadData.resize(settings.MaxThreads);

    gProfilerGeneration++;

    for (uint32_t iZero = 0; iZero < settings.MaxThreads; iZero++)
    {
        ThreadData* threadData = &gProfilerData->threadData[iZero];
        threadData->initialized = iZero == 0;
        threadData->maxLevel = 0;
        threadData->minTime = std::numeric_limits<int64_t>::max();
        threadData->maxTime = 0;
        threadData->currentEntry = 0;
        threadData->name = std::string("Thread ") + std::to_string(iZero);
        threadData->entries.resize(settings.MaxEntriesPerThread);
        memset(threadData->entries.data(), 0, sizeof(ProfilerEntry) * threadData->entries.size());
        threadData->entryStack.resize(50);
        threadData->callStackDepth = 0;
    }

    gProfilerData->frameData.resize(settings.MaxFrames);
    gProfilerData->regionData.resize(settings.MaxRegions);
    for (auto& frame : gProfilerData->frameData)
    {
        frame.frameThreads.resize(settings.MaxThreads);
        frame.frameThreadCount = 0;
        for (auto& threadInfo : frame.frameThreads)
        {
            threadInfo.activeEntry = 0;
            threadInfo.threadIndex = 0;
        }
    }

    gThreadIndexTLS = 0;
    gGenerationTLS = 0;
    gProfilerData->threadData[0].initialized = true;
    gRestarting = true;
    gProfilerData->currentFrame = 0;
    gProfilerData->currentRegion = 0;
    gProfilerData->maxFrameTime = duration_cast<nanoseconds>(milliseconds(30)).count();
    gVisibleFrames = glm::uvec2(0, 0);
    gFrameCandleRange = glm::vec2(0, 0);
    gFrameDisplayStart = 0;
    gRegionDisplayStart = 0;
    gMaxThreadNameSize = 0.0f;
    timer_start(gTimer);

    gPaused = false;
}

void UnDump(std::shared_ptr<ProfilerData>& data)
{
    gPaused = true;
    gRequestPause = true;
    gHideUI = true;
    std::this_thread::sleep_for(1s);

    std::unique_lock<std::mutex> lk(gMutex);
    Init();
    gPaused = true;
    gRequestPause = true;
    gProfilerData = data;

    gHideUI = false;
}

void InitThread()
{
    std::unique_lock<std::mutex> lk(gMutex);

    for (uint32_t iThread = 0; iThread < gProfilerData->threadData.size(); iThread++)
    {
        ThreadData* threadData = &gProfilerData->threadData[iThread];
        if (!threadData->initialized)
        {
            // use it
            gThreadIndexTLS = iThread;
            gGenerationTLS = gProfilerGeneration;
            threadData->currentEntry = 0;
            threadData->initialized = true;

            return;
        }
    }
    //assert(false && "Every thread slots are used!");
}

void FinishThread()
{
    std::unique_lock<std::mutex> lk(gMutex);
    assert(gThreadIndexTLS > -1 && "Trying to finish an uninitilized thread.");
    gProfilerData->threadData[gThreadIndexTLS].initialized = false;
    gThreadIndexTLS = -1;
}

void Finish()
{
    gProfilerData->threadData.clear();
}

void SetPaused(bool pause)
{
    if (gPaused != pause)
    {
        gRequestPause = pause;
    }
}

ThreadData* GetThreadData()
{
    if (gGenerationTLS != gProfilerGeneration.load())
    {
        gThreadIndexTLS = -1;
    }

    if (gThreadIndexTLS == -1)
    {
        InitThread();
    }

    if (gThreadIndexTLS == -1)
    {
        return nullptr;
    }
    return &gProfilerData->threadData[gThreadIndexTLS];
}

void HideThread()
{
    if (gPaused)
    {
        return;
    }
    ThreadData* threadData = GetThreadData();
    threadData->hidden = true;
}

void Reset()
{
    std::unique_lock<std::mutex> lk(gMutex);
    gThreadIndexTLS = -1;
    Init();
}

bool CheckEndState()
{
    if (gPaused)
    {
        return true;
    }

    if (gProfilerData->threadData[gThreadIndexTLS].currentEntry >= settings.MaxEntriesPerThread)
    {
        gPaused = true;
        gRequestPause = true;
    }

    if (gProfilerData->currentFrame >= settings.MaxFrames)
    {
        gPaused = true;
        gRequestPause = true;
    }

    if (gProfilerData->currentRegion >= settings.MaxRegions)
    {
        gPaused = true;
        gRequestPause = true;
    }
    return gPaused;
}

void PushSectionBase(const char* szSection, unsigned int color, const char* szFile, int line)
{
    if (gPaused)
    {
        return;
    }

    ThreadData* threadData = GetThreadData();
    if (CheckEndState())
    {
        return;
    }

    assert(threadData->callStackDepth < settings.MaxCallStack && "Might need to make call stack bigger!");

    // check again
    if (gPaused)
    {
        return;
    }

    // Write entry 0
    ProfilerEntry* profilerEntry = &threadData->entries[threadData->currentEntry];
    threadData->entryStack[threadData->callStackDepth] = threadData->currentEntry;

    if (threadData->callStackDepth > 0)
    {
        profilerEntry->parent = threadData->entryStack[threadData->callStackDepth - 1];
        assert(profilerEntry->parent < threadData->currentEntry);
    }
    else
    {
        profilerEntry->parent = 0xFFFFFFFF;
    }

    assert(szFile != NULL && "No file string specified");
    assert(szSection != NULL && "No section name specified");

    profilerEntry->color = color;
    profilerEntry->szFile = szFile;
    profilerEntry->szSection = szSection;
    profilerEntry->line = line;
    profilerEntry->startTime = timer_get_elapsed(gTimer).count();
    profilerEntry->endTime = std::numeric_limits<int64_t>::max();
    profilerEntry->level = threadData->callStackDepth;
    threadData->callStackDepth++;
    threadData->currentEntry++;

    threadData->maxLevel = std::max(threadData->maxLevel, threadData->callStackDepth);

    threadData->minTime = std::min(profilerEntry->startTime, threadData->minTime);
    threadData->maxTime = std::max(profilerEntry->startTime, threadData->maxTime);

    // New thread begin during frame
    if (threadData->currentEntry == 1)
    {
        // Add this thread to the current frame info
        if (gProfilerData->currentFrame > 0)
        {
            auto& frame = gProfilerData->frameData[gProfilerData->currentFrame - 1];
            auto& threadInfo = frame.frameThreads[frame.frameThreadCount];
            threadInfo.activeEntry = threadData->currentEntry - 1;
            threadInfo.threadIndex = gThreadIndexTLS;
            frame.frameThreadCount++;
        }
    }

    if (gRestarting)
    {
        gRestarting = false;
    }
}

void PopSection()
{
    if (gPaused)
    {
        return;
    }

    ThreadData* threadData = GetThreadData();
    if (CheckEndState())
    {
        return;
    }

    if (gRestarting || gPaused)
    {
        return;
    }

    if (threadData->callStackDepth <= 0)
    {
        return;
    }

    // Back to the last entry we wrote
    threadData->callStackDepth--;

    int entryIndex = threadData->entryStack[threadData->callStackDepth];
    ProfilerEntry* profilerEntry = &threadData->entries[entryIndex];

    assert(profilerEntry->szSection != nullptr);

    // store end time
    profilerEntry->endTime = timer_get_elapsed(gTimer).count();

    threadData->maxTime = std::max(profilerEntry->endTime, threadData->maxTime);
}

void SetRegionLimit(uint64_t maxTimeNs)
{
    gProfilerData->regionTimeLimit = maxTimeNs;
}

void NameThread(const char* pszName)
{
    if (gPaused)
    {
        return;
    }

    // Must get thread data to init the thread
    ThreadData* threadData = GetThreadData();
    if (!threadData)
    {
        return;
    }
    threadData->name = pszName;
}

// You are allowed one secondary region - I use it for audio thread monitoring
void BeginRegion()
{
    if (gPaused)
    {
        return;
    }

    // Must get thread data to init the thread
    ThreadData* threadData = GetThreadData();
    if (CheckEndState())
    {
        return;
    }

    auto& region = gProfilerData->regionData[gProfilerData->currentRegion];
    region.startTime = timer_get_elapsed(gTimer).count();
}

void EndRegion()
{
    if (gPaused)
    {
        return;
    }

    ThreadData* threadData = GetThreadData();
    if (CheckEndState())
    {
        return;
    }

    auto& region = gProfilerData->regionData[gProfilerData->currentRegion];
    region.endTime = timer_get_elapsed(gTimer).count();
   
    // Reconstructed at display time, not capture time!!
    //region.name = std::format("{:.2f}ms", float(timer_to_ms(nanoseconds(region.endTime - region.startTime))));

    gProfilerData->currentRegion++;
}

void NewFrame()
{
    if (gPaused)
    {
        return;
    }

    if (CheckEndState())
    {
        return;
    }

    auto& frame = gProfilerData->frameData[gProfilerData->currentFrame];
    for (uint32_t threadIndex = 0; threadIndex < settings.MaxThreads; threadIndex++)
    {
        auto& thread = gProfilerData->threadData[threadIndex];
        if (!thread.initialized)
        {
            continue;
        }
        if (thread.currentEntry > 0)
        {
            // Remember which entry was active for this thread
            auto& threadInfo = frame.frameThreads[frame.frameThreadCount];
            threadInfo.activeEntry = thread.currentEntry - 1;
            threadInfo.threadIndex = threadIndex;

            // Next thread
            frame.frameThreadCount++;
        }
    }

    frame.startTime = timer_get_elapsed(gTimer).count();
    if (gProfilerData->currentFrame > 0)
    {
        gProfilerData->frameData[gProfilerData->currentFrame - 1].endTime = frame.startTime;
        gProfilerData->frameData[gProfilerData->currentFrame - 1].name = std::format("{:.2f}ms", float(timer_to_ms(nanoseconds(frame.startTime - gProfilerData->frameData[gProfilerData->currentFrame - 1].startTime))));
    }
    gProfilerData->currentFrame++;
}

// Which frames we can see in the main viewport for the current zoom
void UpdateVisibleFrameRange()
{
    if (gProfilerData->currentFrame < 2)
    {
        gVisibleFrames.x = gVisibleFrames.y = 0;
        gFrameCandleRange.x = gFrameCandleRange.y = 0.0f;
    }

    glm::i32vec2 frameRangeLimits = glm::i32vec2(0, gProfilerData->currentFrame - 2);

    gVisibleFrames.x = std::clamp(gVisibleFrames.x, 0, frameRangeLimits.x);
    gVisibleFrames.y = std::clamp(gVisibleFrames.y, 0, frameRangeLimits.y);

    while ((gVisibleFrames.y < frameRangeLimits.y) && (gProfilerData->frameData[gVisibleFrames.y].endTime < gTimeRange.y))
    {
        gVisibleFrames.y++;
    };

    while ((gVisibleFrames.y > frameRangeLimits.x) && (gProfilerData->frameData[gVisibleFrames.y].startTime > gTimeRange.y))
    {
        gVisibleFrames.y--;
    };

    while ((gVisibleFrames.x < frameRangeLimits.y) && (gProfilerData->frameData[gVisibleFrames.x].endTime < gTimeRange.x))
    {
        gVisibleFrames.x++;
    };

    while ((gVisibleFrames.x > frameRangeLimits.x) && (gProfilerData->frameData[gVisibleFrames.x].startTime > gTimeRange.x))
    {
        gVisibleFrames.x--;
    };
    gVisibleFrames.y++;
}

// Show the frame and region candles at the top.
// Returns possible selected time range from the mouse
glm::u64vec2 ShowCandles(glm::vec2& regionMin, glm::vec2& regionMax)
{
    // Show the frame candles
    auto handleMouse = [&](const char* buttonName, const NRectf& region, auto& range, const auto& currentFrame) {
        const auto minCandlesPerView = int(4);
        const glm::vec2 regionSize = region.Size();
        bool changed = false;

        if (!gPaused)
        {
            gCandleDragRect.Clear();
            return false;
        }

        // This code could be abstracted into a generalized zoom and perhaps shared with the similar code below
        ImGui::InvisibleButton(buttonName, regionSize);
        if (ImGui::IsItemActive())
        {
            if (ImGui::IsMouseDragging(0))
            {
                auto delta = ImGui::GetMouseDragDelta(0).x;
                if (ImGui::GetIO().KeyCtrl)
                {
                    gCandleDragRect = NRectf(glm::vec2(ImGui::GetIO().MouseClickedPos[0]) - region.topLeftPx, glm::vec2(ImGui::GetIO().MouseClickedPos[0]) - region.topLeftPx + glm::vec2(ImGui::GetMouseDragDelta(0)));
                    gCandleDragRect.Normalize();
                }
                else
                {
                    gCandleDragRect = NRectf();
                    auto dragCandleDelta = float((delta / regionSize.x) * (range.y - range.x));
                    ImGui::ResetMouseDragDelta(0);

                    auto newVisible = range - glm::vec2(dragCandleDelta, dragCandleDelta);
                    if ((newVisible.y < (currentFrame - 1) && newVisible.x >= MinFrame))
                    {
                        range = newVisible;
                        changed = true;
                    }
                }
            }
            else if (ImGui::IsMouseClicked(0))
            {
                gCandleDragRect.Clear();
            }
        }

        // Zoom and mouse click over area
        if (ImGui::IsMouseHoveringRect(region.topLeftPx, region.bottomRightPx))
        {
            if (ImGui::IsMouseReleased(0))
            {
                if (gCandleDragRect.Empty())
                {
                    gCandleDragRect = NRectf(glm::vec2(ImGui::GetIO().MouseClickedPos[0]) - region.topLeftPx, glm::vec2(ImGui::GetIO().MouseClickedPos[0]) - region.topLeftPx + glm::vec2(1.0f, 0.0f));
                    gCandleDragRect.Normalize();
                }
            }

            if (ImGui::GetIO().MouseWheel != 0)
            {
                gCandleDragRect.Clear();

                const auto sectionWheelZoomSpeed = 1.0f;
                auto mouseToCandle = [&](glm::vec2& range) {
                    return (((ImGui::GetMousePos().x - region.Left()) / regionSize.x) * (range.y - range.x)) + range.x;
                };

                auto zoom = ImSign(ImGui::GetIO().MouseWheel) * sectionWheelZoomSpeed;
                if (zoom != 0.0f)
                {
                    auto candleRange = range.y - range.x;
                    auto tenPercent = ((range.y - range.x) * .1f) * zoom;
                    auto newVisible = range + glm::vec2(tenPercent, -tenPercent);

                    auto oldMouseCandle = mouseToCandle(range);
                    auto newMouseCandle = mouseToCandle(newVisible);

                    newVisible.x -= (newMouseCandle - oldMouseCandle);
                    newVisible.y -= (newMouseCandle - oldMouseCandle);

                    if ((newVisible.y - newVisible.x) >= minCandlesPerView)
                    {
                        range = newVisible;
                        if (range.x < MinFrame)
                        {
                            auto diff = MinFrame - range.x;
                            range.x += diff;
                            range.y += diff;
                        }

                        if (range.y > int32_t(currentFrame - 1))
                        {
                            auto diff = range.y - (currentFrame - 1);
                            range.x -= diff;
                            range.y -= diff;
                        }
                        range.x = std::clamp(range.x, float(MinFrame), float(currentFrame - 1));
                        range.y = std::clamp(range.y, float(MinFrame), float(currentFrame - 1));
                        changed = true;
                    }
                }
            }
        }
        return changed;
    };

    const float CandleHeight = 30 * dpi.scaleFactorXY.y;
    NRectf regionFrames = NRectf(regionMin.x, regionMin.y, regionMax.x - regionMin.x, CandleHeight);
    NRectf regionRegion = NRectf(regionMin.x, regionMin.y + CandleHeight, regionMax.x - regionMin.x, CandleHeight);
    NRectf regionBoth = NRectf(regionFrames.topLeftPx, regionRegion.bottomRightPx);

    handleMouse("##frameButton", regionBoth, gFrameCandleRange, gProfilerData->currentFrame);

    if (!gPaused)
    {
        if (gProfilerData->currentFrame >= MinLeadInFrames)
        {
            gFrameCandleRange = glm::vec2(MinFrame, float(gProfilerData->currentFrame - 1));
            gFrameCandleRange.x = std::max(gFrameCandleRange.x, 0.0f);
        }
    }

    const auto& settings = GlobalSettingsManager::Instance();
    auto theme = settings.GetCurrentTheme();

    glm::u64vec2 dragTimeRange = glm::u64vec2(0);
    auto drawRegions = [&](const auto maxRegion, const auto& region, const auto& framesStartTime, const auto& framesDuration, auto& regionData, auto& regionDisplayStart, const auto& maxTime, const auto& limitTime, const auto& color1, const auto& color2) {
        const glm::vec2 candleRegionSize = region.Size();
        const auto pDrawList = ImGui::GetWindowDrawList();
        const auto MaxCandleColor = settings.GetVec4f(theme, c_Error, glm::vec4(1.0f, 0.1f, 0.1f, 1.0f));

        auto timePerPixel = framesDuration / int64_t(region.Width());

        regionDisplayStart = std::min(regionDisplayStart, int64_t(maxRegion - 1));
        regionDisplayStart = std::max(regionDisplayStart, int64_t(0));

        // Keep global counters to simplify finding the regions
        while (regionDisplayStart > 0 && regionData[regionDisplayStart].startTime > framesStartTime)
        {
            regionDisplayStart--;
        }
        while ((regionDisplayStart < maxRegion) && regionData[regionDisplayStart].endTime < framesStartTime)
        {
            regionDisplayStart++;
        }

        auto dragLimits = glm::vec2(gCandleDragRect.topLeftPx.x + region.Left(), gCandleDragRect.Right() + region.Left());
        auto pixelTime = framesStartTime - timePerPixel;
        auto currentRegion = regionDisplayStart;
        auto lastX = -1.0f;
        auto pendingCandleHeight = 0.0f;
        auto pendingColorLerp = 0.0f;
        auto colOn = (currentRegion & 0x1) ? true : false;
        auto regionFind = region.Contains(ImGui::GetIO().MouseClickedPos[0]) && (dragLimits.x != dragLimits.y);

        // Walk the pixels
        for (float pixel = region.Left(); pixel < region.Right(); pixel++)
        {
            // Start of pixel time
            pixelTime += timePerPixel;

            // Catch up to the pixel
            while ((currentRegion < maxRegion) && (regionData[currentRegion].endTime < pixelTime))
            {
                currentRegion++;
            }

            // Don't fall off the end - if we do, then there are no regions to draw
            if (currentRegion >= maxRegion)
            {
                lastX = -1;
                break;
            }

            // We are ahead, move to next pixel
            if (regionData[currentRegion].startTime > (pixelTime + timePerPixel))
            {
                // Draw the last thing first
                if (lastX != -1 && pendingCandleHeight != 0.0f)
                {
                    // Should probably blend here
                    auto col = colOn ? color1 : color2;
                    colOn = !colOn;

                    col = Zest::Mix(col, MaxCandleColor, pendingColorLerp);

                    auto minRect = ImVec2(lastX, region.Bottom() - 1.0f - std::max(1.0f, (pendingCandleHeight * region.Height() - 2.0f)));
                    auto maxRect = ImVec2(pixel, region.Bottom() - 1.0f);
                    pDrawList->AddRectFilled(minRect, maxRect, ToPackedABGR(col));
                }

                // Remember we aren't in a region now
                lastX = -1;
                pendingCandleHeight = 0.0f;
                continue;
            }

            if (currentRegion < maxRegion)
            {
                glm::u64vec2 regionTimeRange;
                regionTimeRange.x = regionData[int64_t(currentRegion)].startTime;

                // Collect durations of all candles within this pixel
                uint32_t count = 0;
                float totalDuration = 0.0;
                while (currentRegion < maxRegion)
                {
                    auto& data = regionData[int64_t(currentRegion)];
                    regionTimeRange.y = data.endTime;

                    glm::u64vec2 overlap;
                    overlap.x = std::max(data.startTime, pixelTime);
                    overlap.y = std::min(data.endTime, pixelTime + timePerPixel);

                    float overlapRatio = 0.0f;
                    if (overlap.y > overlap.x)
                    {
                        overlapRatio = (overlap.y - overlap.x) / float(timePerPixel);
                        totalDuration += (data.endTime - data.startTime) * overlapRatio;
                        count++;
                    }

                    // Find the time region which the mouse has selected
                    if (regionFind)
                    {
                        if (dragTimeRange.x == 0 && (dragLimits.x < pixel))
                        {
                            dragTimeRange.x = regionTimeRange.x;
                            dragTimeRange.y = regionTimeRange.y;
                        }
                        if (dragLimits.y >= pixel)
                        {
                            dragTimeRange.y = regionTimeRange.y;
                        }
                    }

                    if (regionData[currentRegion].endTime > (pixelTime + timePerPixel))
                    {
                        break;
                    }
                    currentRegion++;
                }

                // Found something, draw average
                if (count > 0)
                {
                    totalDuration /= count;
                    float candleHeight = totalDuration / float(maxTime);
                    candleHeight = std::min(candleHeight, 1.0f);

                    float candleColorLerp = totalDuration / float(limitTime);
                    candleColorLerp = std::clamp(candleColorLerp, 0.0f, 1.0f);

                    if (lastX == -1)
                    {
                        lastX = pixel;
                    }
                    else
                    {
                        if (pendingCandleHeight != candleHeight)
                        {
                            // Should probably blend here
                            auto col = colOn ? color1 : color2;
                            colOn = !colOn;

                            col = Zest::Mix(col, MaxCandleColor, pendingColorLerp);

                            auto minRect = ImVec2(lastX, region.Bottom() - 1.0f - std::max(1.0f, (pendingCandleHeight * region.Height() - 2.0f)));
                            auto maxRect = ImVec2(pixel, region.Bottom() - 1.0f);
                            pDrawList->AddRectFilled(minRect, maxRect, ToPackedABGR(col));

                            if (ImGui::IsMouseHoveringRect(minRect, maxRect))
                            {
                                auto tip = std::format("{}: {:.4f}%", currentRegion, ((maxRect.y - minRect.y) / region.Height()) * 100.0f);
                                ImGui::SetTooltip("%s", tip.c_str());
                            }

                            lastX = -1;
                        }
                    }
                    pendingCandleHeight = candleHeight;
                    pendingColorLerp = candleColorLerp;
                }
                else
                {
                    lastX = -1;
                }
            }
        }

        if (lastX != -1)
        {
            // Should probably blend here
            auto col = colOn ? color1 : color2;
            col = Zest::Mix(col, MaxCandleColor, pendingColorLerp);

            pDrawList->AddRectFilled(ImVec2(lastX, region.Bottom() - 1.0f - std::max(1.0f, (pendingCandleHeight * region.Height() - 2.0f))), ImVec2(region.Right(), region.Bottom() - 1.0f), ToPackedABGR(col));
        }

        if (!gCandleDragRect.Empty())
        {
            pDrawList->AddRectFilled(ImVec2(gCandleDragRect.Left() + region.topLeftPx.x, region.topLeftPx.y), ImVec2(gCandleDragRect.Right() + region.topLeftPx.x, region.Bottom()), 0x77777777);
        }

        assert(dragTimeRange.x <= dragTimeRange.y);
    };

    const auto FrameCandleColor = settings.GetVec4f(theme, c_AccentColor1, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
    const auto FrameCandleAltColor = settings.GetVec4f(theme, c_AccentColor2, glm::vec4(1.0f, 0.4f, 0.4f, 1.0f));
    const auto RegionCandleColor = settings.GetVec4f(theme, c_Warning, glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
    const auto RegionCandleAltColor = RegionCandleColor * 0.8f;
    const auto framesStartTime = gProfilerData->frameData[int64_t(gFrameCandleRange.x)].startTime;
    const auto framesDuration = gProfilerData->frameData[int64_t(gFrameCandleRange.y)].startTime - framesStartTime;

    drawRegions(gProfilerData->currentFrame, regionFrames, framesStartTime, framesDuration, gProfilerData->frameData, gFrameDisplayStart, gProfilerData->maxFrameTime, gProfilerData->maxFrameTime, FrameCandleColor, FrameCandleAltColor);
    regionMin.y += CandleHeight + 2.0f * dpi.scaleFactorXY.y;

    drawRegions(gProfilerData->currentRegion, regionRegion, framesStartTime, framesDuration, gProfilerData->regionData, gRegionDisplayStart, gProfilerData->regionTimeLimit, gProfilerData->regionTimeLimit, RegionCandleColor, RegionCandleAltColor);
    regionMin.y += CandleHeight;

    if (dragTimeRange.x > dragTimeRange.y)
    {
        dragTimeRange = glm::u64vec2(0);
    }
    return dragTimeRange;
}

// Show the profiler window
void ShowProfile()
{
    if (gHideUI)
    {
        return;
    }
    PROFILE_SCOPE(Profile_UI);

    bool pause = gPaused;
    bool changed = false;

    if (pause != gRequestPause)
    {
        changed = true;
        pause = gRequestPause;
    }

    if (ImGui::Button(pause ? "Resume" : "Pause"))
    {
        changed = true;
        pause = !pause;
    }

    if (changed)
    {
        // Call reset before setting paused
        if (!pause)
        {
            Reset();
        }
        gPaused = pause;
        gRequestPause = pause;
    }

    ImGui::SameLine();

    static float scale = 1.0f;

    ImGui::PushItemWidth(100 * dpi.scaleFactorXY.x);
    ImGui::SliderFloat("Scale", &scale, .5f, 2.0f, "%.2f");

    ImGui::SameLine();

    ImGui::TextUnformatted(std::format("  UI FPS {:.1f}", ImGui::GetIO().Framerate).c_str());

    // Ignore the first frame, which is likely a long delay due to
    // the time that expires after this profiler is created and the first
    // frame is drawn
    const uint32_t MaxFrame = gProfilerData->currentFrame - 2;
    if (gProfilerData->currentFrame < MinLeadInFrames)
    {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    auto regionSize = ImGui::GetContentRegionAvail();
    glm::vec2 regionMin(ImGui::GetCursorScreenPos());
    glm::vec2 regionMax(regionMin.x + regionSize.x, regionMin.y + regionSize.y);
    glm::vec2 topLeft = regionMin;

    auto pDrawList = ImGui::GetWindowDrawList();

    // Always background fill the profiler
    pDrawList->AddRectFilled(ImVec2(regionMin.x, regionMin.y), ImVec2(regionMax.x, regionMax.y), 0xFF111111);

    auto selectedTimeRange = ShowCandles(regionMin, regionMax);

    // Reset region size
    regionSize = regionMax - regionMin;

    // Setup
    const glm::vec2 textPadding = glm::vec2(3, 3) * dpi.scaleFactorXY;
    const auto pFont = ImGui::GetFont();
    const auto fontSize = ImGui::GetFontSize() * scale;
    const auto smallFontSize = fontSize * .66f;
    const auto heightPerLevel = fontSize + 2.0f;

    const auto maxTime = gProfilerData->frameData[gProfilerData->currentFrame - 1].startTime;
    const auto minTime = gProfilerData->frameData[MinFrame].startTime;
    int64_t visibleDuration;
    double pixelsPerTime;
    double timePerPixels;

    auto setTimeRange = [&](glm::i64vec2 range) {
        assert(range.y >= range.x);
        if ((range.y < range.x) || ((range.y - range.x) < 1000))
        {
            range.x = std::clamp(range.x, minTime, maxTime - 1000);
            range.y = range.x + 1000;
        }

        if (range.x < minTime)
        {
            auto diff = minTime - range.x;
            range.x += diff;
            range.y += diff;
        }
        else if (range.y > maxTime)
        {
            auto diff = range.y - maxTime;
            range.y -= diff;
            range.x -= diff;
        }
        range.x = std::clamp(range.x, minTime, maxTime);
        range.y = std::clamp(range.y, minTime, maxTime);
        assert((range.y - range.x) >= 1000);

        gTimeRange = range;
        visibleDuration = range.y - range.x;
        pixelsPerTime = (double)regionSize.x / double(visibleDuration);
        timePerPixels = 1.0 / pixelsPerTime;
    };

    const auto now = (int64_t)timer_get_elapsed(gTimer).count();
    if (!gPaused)
    {
        auto duration = duration_cast<nanoseconds>(milliseconds(50)).count();
        setTimeRange(glm::i64vec2(now - duration, now));
    }
    else
    {
        if (selectedTimeRange.x != selectedTimeRange.y)
        {
            // Add 10 percent to the selection
            auto tenPercent = int64_t((selectedTimeRange.y - selectedTimeRange.x) * .05f);
            selectedTimeRange.x -= tenPercent;
            selectedTimeRange.y += tenPercent;

            gTimeRange = selectedTimeRange;
        }
        setTimeRange(gTimeRange);
    }

    // Time normalized to gTimeRange.x
    auto xFromTime = [&](int64_t time) {
        return ((time - gTimeRange.x) * regionSize.x) / visibleDuration;
    };

    auto timeFromX = [&](uint32_t x) {
        return gTimeRange.x + double(x / regionSize.x) * visibleDuration;
    };

    //regionSize.y -= 2; Why was this here?

    // Seen in testing, region size tiny causes an exception/assert in Invisible Button
    if (regionSize.y <= 0.0f || regionSize.x <= 0.0f)
    {
        ImGui::PopStyleVar(1);
        return;
    }

    glm::vec2 mouseClick = glm::vec2(0.0f);
    ImGui::InvisibleButton("##FramesSectionsWindowDummy", regionSize);
    if (ImGui::IsItemActive())
    {
        if (ImGui::IsMouseDragging(0))
        {
            gCandleDragRect.Clear();

            auto delta = ImGui::GetMouseDragDelta(0).x;
            auto dragTimeDelta = int64_t((delta / regionSize.x) * visibleDuration);
            ImGui::ResetMouseDragDelta(0);
            auto newTime = glm::i64vec2(gTimeRange.x - dragTimeDelta, gTimeRange.y - dragTimeDelta);
            if ((newTime.y < maxTime && newTime.x >= minTime))
            {
                setTimeRange(newTime);
            }
        }
        else if (ImGui::IsMouseClicked(0))
        {
            mouseClick = ImGui::GetMousePos();
            gCandleDragRect.Clear();
        }
    }

    // Zoom
    if (ImGui::IsMouseHoveringRect(regionMin, regionMax) && ImGui::GetIO().MouseWheel != 0)
    {
        gCandleDragRect.Clear();

        const auto sectionWheelZoomSpeed = 1.0f;
        auto zoom = ImSign(ImGui::GetIO().MouseWheel) * sectionWheelZoomSpeed;
        auto localMousePos = uint32_t(ImGui::GetMousePos().x - regionMin.x);
        auto tenPercent = (int64_t)((gTimeRange.y - gTimeRange.x) * .1 * zoom);

        auto mouseTime = timeFromX(localMousePos);
        auto newTime = glm::i64vec2((gTimeRange.x + tenPercent), gTimeRange.y - tenPercent);

        setTimeRange(newTime);

        auto newMouseTime = timeFromX(localMousePos);
        auto timeDelta = int64_t(newMouseTime - mouseTime);
        newTime = glm::i64vec2(gTimeRange.x - timeDelta, gTimeRange.y - timeDelta);
        setTimeRange(newTime);
    }

    UpdateVisibleFrameRange();

    // Walk the visible frames, drawing each thread
    double lastFrameX = -regionSize.x;
    bool firstFrame = true;

    for (int32_t frameIndex = gVisibleFrames.x; frameIndex < gVisibleFrames.y; frameIndex++)
    {
        auto& frameInfo = gProfilerData->frameData[frameIndex];

        // Left hand side of the frame
        auto xFrameMarker = xFromTime(frameInfo.startTime);
        if (xFrameMarker >= 0.0 && (xFrameMarker - lastFrameX) > 20.0)
        {
            // Vertical frame line
            pDrawList->AddLine(ImVec2(regionMin.x + float(xFrameMarker), regionMin.y), ImVec2(regionMin.x + float(xFrameMarker), regionMax.y), frameMarkerColor, 1.0f);

            // Frame text
            frameInfo.name = std::format("F{}: {:.2f}ms", frameIndex, float(timer_to_ms(nanoseconds(frameInfo.endTime - frameInfo.startTime))));
            if ((xFrameMarker - lastFrameX) > ImGui::CalcTextSize(frameInfo.name.c_str()).x)
            {
                pDrawList->AddText(pFont, smallFontSize, ImVec2(regionMin.x + float(xFrameMarker) + textPadding.x, regionMin.y + textPadding.y), 0xFFAAAAAA, frameInfo.name.c_str(), NULL, 0.0f, nullptr);
            }
            lastFrameX = xFrameMarker;
        }

        float y = regionMin.y + smallFontSize + textPadding.y;

        for (uint32_t threadIndex = 0; threadIndex < gProfilerData->frameData[frameIndex].frameThreadCount; threadIndex++)
        {
            auto& frameThreadInfo = frameInfo.frameThreads[threadIndex];
            auto& threadData = gProfilerData->threadData[frameThreadInfo.threadIndex];

            if (threadData.hidden)
            {
                continue;
            }

            float threadHeight = (heightPerLevel * threadData.maxLevel) + textPadding.y * 2.0f;

            assert(threadData.initialized);

            if (firstFrame)
            {
                pDrawList->AddRectFilled(ImVec2(regionMin.x, y), ImVec2(regionMax.x, y + threadHeight), gSelectedThread == threadIndex ? 0xFF333333 : 0xFF111111);
                pDrawList->AddLine(ImVec2(regionMin.x, y), ImVec2(regionMax.x, y), 0xFF333333);
            }

            y += textPadding.y;

            auto showEntry = [&](uint32_t index) {
                auto& entry = threadData.entries[index];

                // Ignore wholly outside our visible range
                if (entry.startTime > gTimeRange.y || entry.endTime < gTimeRange.x)
                {
                    return;
                }

                float yEntry = y + entry.level * heightPerLevel;
                float xEntry = float(xFromTime(entry.startTime));
                float xEnd = float(xFromTime(entry.endTime));

                // Avoid alliasing/make it easy to see small entries
                if (xEnd < (xEntry + 1))
                {
                    xEnd = xEntry + 1;
                }

                ImVec2 rectMin(std::max(xEntry + regionMin.x, regionMin.x), yEntry);
                ImVec2 rectMax(std::min(xEnd + regionMin.x, regionMax.x), yEntry + heightPerLevel);
                pDrawList->AddRectFilled(rectMin, rectMax, entry.color | 0xFF000000);

                if (ImGui::IsMouseHoveringRect(rectMin, rectMax))
                {
                    auto tip = std::format("{}: {:.4f}ms ({:.2f}us)\nRange: {:.4f}ms - {:.4f}ms\n\n{} (Ln {})", entry.szSection, timer_to_ms(nanoseconds(std::min(entry.endTime, threadData.maxTime) - entry.startTime)), (std::min(entry.endTime, threadData.maxTime) - entry.startTime) / 1000.0f, timer_to_ms(nanoseconds(entry.startTime)), timer_to_ms(nanoseconds(entry.endTime)), entry.szFile, entry.line);
                    ImGui::SetTooltip("%s", tip.c_str());
                }

                float width = rectMax.x - rectMin.x;
                auto clip = ImVec4(rectMin.x, rectMin.y, rectMax.x, rectMax.y);
                auto textSize = ImGui::CalcTextSize(entry.szSection);

                // Center the text if possible
                float textPos = textPadding.x + rectMin.x;
                if (textSize.x < width)
                {
                    textPos += (width - textSize.x) * .5f;
                }

                if (width > MinSizeForTextDisplay)
                {
                    pDrawList->AddText(pFont, fontSize, ImVec2(textPos, yEntry + textPadding.y), LuminanceARGB(entry.color) > .5f ? 0xFF000000 : 0xFFFFFFFF, entry.szSection, NULL, 0.0f, &clip);
                }
            };

            // Get the most recent thread entry for this frame
            auto currentEntry = frameThreadInfo.activeEntry;
            if (currentEntry == 0)
            {
                continue;
            }

            // Walk up the stack to find the outer parent; since it might have started before this frame
            while (threadData.entries[currentEntry].parent != 0xFFFFFFFF)
            {
                auto last = currentEntry;
                currentEntry = threadData.entries[currentEntry].parent;

                // WAR for a noticed lockup; needs to be debugged.
                if (last == currentEntry)
                {
                    assert(!"This shouldn't happen");
                    break;
                }
            }

            // Step back to find entries that began before the frame
            while (currentEntry > 0 && threadData.entries[currentEntry].endTime > frameInfo.startTime)
            {
                currentEntry--;
            }

            while ((currentEntry < (threadData.currentEntry - 1)) && threadData.entries[currentEntry].startTime < frameInfo.endTime)
            {
                showEntry(currentEntry);
                currentEntry++;
            }

            if (firstFrame && mouseClick.y >= y && mouseClick.y <= (y + threadHeight))
            {
                if (gSelectedThread != threadIndex)
                {
                    gSelectedThread = threadIndex;
                }
                else
                {
                    gSelectedThread = -1;
                }
            }

            // Draw the text over the top
            gMaxThreadNameSize = std::max(ImGui::CalcTextSize(threadData.name.c_str()).x, gMaxThreadNameSize);
            pDrawList->AddRectFilled(ImVec2(regionMin.x, y + threadHeight - textPadding.y - smallFontSize), ImVec2(regionMin.x + gMaxThreadNameSize, y + threadHeight - textPadding.y), gSelectedThread == threadIndex ? 0xFF333333 : 0xFF111111);
            pDrawList->AddText(pFont, smallFontSize, ImVec2(regionMin.x + textPadding.x, y + threadHeight - smallFontSize - textPadding.y), 0xFFAAAAAA, threadData.name.c_str(), NULL, 0.0f, nullptr);

            // Next thread
            y += heightPerLevel * threadData.maxLevel + textPadding.y;
        }
        firstFrame = false;
    }

    ImGui::PopStyleVar(1);
}

const glm::vec4& ColorFromName(const char* pszName, const uint32_t len)
{
    const auto col = murmur_hash(pszName, len, 0);
    return DefaultColors[col % NUM_DEFAULT_COLORS];
}

} // namespace Profiler
} // namespace Zest
