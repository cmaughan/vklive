#pragma once

struct WindowEnables
{
    bool profiler = false;
    bool targets = false;
};

extern WindowEnables g_WindowEnables;

bool menu_show(); // return true if menu item is active
