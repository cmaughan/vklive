#pragma once

#include <cassert>
#include <thread>

namespace vklive_nvim
{

// ---------------------------------------------------------------------------
// MainThreadChecker — lightweight debug-only utility that captures the creating
// thread's ID and provides assert_main_thread() to verify callers are on that
// thread.  All checks compile to no-ops in release builds (NDEBUG defined).
//
// Usage: embed as a member in any class whose public API must only be called
// from the main thread, then call thread_checker_.assert_main_thread() at the
// top of each guarded method.
// ---------------------------------------------------------------------------
class MainThreadChecker
{
public:
    MainThreadChecker()
#ifndef NDEBUG
        : owner_thread_id_(std::this_thread::get_id())
#endif
    {
    }

    // Asserts that the caller is on the thread that constructed this checker.
    // No-op in release builds.
    void assert_main_thread([[maybe_unused]] const char* context = nullptr) const
    {
#ifndef NDEBUG
        assert(std::this_thread::get_id() == owner_thread_id_
            && (context ? context : "MainThreadChecker: called from wrong thread"));
#endif
    }

private:
#ifndef NDEBUG
    std::thread::id owner_thread_id_;
#endif
};

} // namespace vklive_nvim
