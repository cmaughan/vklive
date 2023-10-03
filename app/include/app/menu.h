#pragma once

struct WindowEnables
{
    bool profiler = false;
    bool targets = false;
    bool sequencer = false;
};

extern WindowEnables g_WindowEnables;

bool menu_show(); // return true if menu item is active
