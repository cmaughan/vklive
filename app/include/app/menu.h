#pragma once

struct WindowEnables
{
    bool profiler = false;
    bool targets = false;
    bool sequencer = false;
    bool demoWindow = false;
};

extern WindowEnables g_WindowEnables;

bool menu_show(); // return true if menu item is active
