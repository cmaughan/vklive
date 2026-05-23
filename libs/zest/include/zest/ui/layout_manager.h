#pragma once

#include <map>
#include <string>
#include <zest/file/file.h>

namespace Zest
{

using LayoutShowFlags = std::map<std::string, bool>;
struct LayoutInfo
{
    std::string windowLayout;
    LayoutShowFlags showFlags;
};

using fnLoadCB = std::function<void(const std::string&, const LayoutInfo&)>;
struct WindowState
{
    std::string name; // For the menu
    bool* pVisible;
};
struct LayoutManagerData
{
    fnLoadCB loadCB;
    fs::path layoutSettingsPath;
    std::map<std::string, LayoutInfo> layouts;
    std::map<std::string, WindowState> mapWindowState;
    std::string pendingLayoutLoad;
    bool popupLayoutSaveRequest;
};

extern LayoutManagerData LayoutData;

// Register a window state to be managed; ensure this is called before the first load of the layouts file
void layout_manager_register_window(const std::string& key, const std::string& name, bool* showState);

// Provide the layout file, and callback when loaded
void layout_manager_load_layouts_file(const std::string& appName, const fnLoadCB& fnLoad, bool forceReset = false);

// Load the given layout
void layout_manager_load_layout(const std::string& layoutName);

// Save everything
void layout_manager_save();

// Called to display the menu, after the menu is finished drawing to show any popups, and once per frame outside of the ImGui::NewFrame
void layout_manager_do_menu();
bool layout_manager_do_menu_popups();
void layout_manager_update();

}; // namespace Zest