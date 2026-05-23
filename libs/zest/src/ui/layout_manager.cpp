
#include <zest/common.h>
#include <zest/file/runtree.h>
#include <zest/file/toml_utils.h>
#include <zest/logger/logger.h>
#include <zest/ui/layout_manager.h>
#include <zest/math/imgui_glm.h>

#include <format>

#include <cppcodec/base64_default_rfc4648.hpp>

#undef ERROR
namespace Zest
{

void layout_manager_save_layouts_file();
void layout_manager_save_layout(const std::string& layoutName, const std::string& layoutText);

LayoutManagerData LayoutData;

void layout_manager_load_layouts_file(const std::string& appName, const fnLoadCB& fnLoad, bool forceReset)
{
    LayoutData.layouts.clear();
    LayoutData.loadCB = fnLoad;
    try
    {
        LayoutData.layoutSettingsPath = file_appdata_path();
        if (LayoutData.layoutSettingsPath.empty())
        {
            LayoutData.layoutSettingsPath = file_documents_path();
            if (LayoutData.layoutSettingsPath.empty())
            {
                LayoutData.layoutSettingsPath = fs::temp_directory_path();
            }
        }

        // TOOD: Error message on no path
        LayoutData.layoutSettingsPath = LayoutData.layoutSettingsPath / appName / "settings" / "layouts.toml";
        if (forceReset || !fs::exists(LayoutData.layoutSettingsPath))
        {
            // Make layouts directory
            fs::create_directories(LayoutData.layoutSettingsPath.parent_path());

            auto flags = fs::copy_options::recursive;
            flags |= fs::copy_options::overwrite_existing;

            // Copy everything from settings/layouts
            auto sourceSettings = runtree_path() / "settings" / "layouts.toml";
            if (fs::exists(sourceSettings))
            {
                fs::copy(sourceSettings, LayoutData.layoutSettingsPath.parent_path(), flags);
                LOG(INFO, "Copied layouts");
            }
            else
            {
                LOG(ERR, "Default setting file not found: " << sourceSettings.string());

                LayoutData.layouts["Default"] = LayoutInfo{};

                return;
            }
        }

        // Read the layouts.toml
        auto settingsData = toml::parse_file(LayoutData.layoutSettingsPath.string());

        if (toml::array* layouts = settingsData["layout"].as_array())
        {
            for (auto& entry : *layouts)
            {
                if (entry.is_table())
                {
                    auto layoutTable = *entry.as_table();
                    LayoutInfo info;

                    // Enables
                    auto enablesTable = layoutTable["enables"].as_table();
                    if (enablesTable)
                    {
                        for (auto& [key, value] : *enablesTable)
                        {
                            if (value.is_boolean())
                            {
                                info.showFlags[key.str().data()] = value.as_boolean()->get();
                            }
                        }
                    }
                
                    std::string base64 = layoutTable["windows"].value_or("");

                    // Decode the base64 back to a layout that the client can understand
                    auto vecData = cppcodec::base64_rfc4648::decode(base64);
                    info.windowLayout = std::string((const char*)vecData.data(), vecData.size());

                    std::string name = layoutTable["name"].value_or("");

                    if (!info.windowLayout.empty())
                    {
                        LayoutData.layouts[name] = info;

                        // Load the 'Default' layout by default
                        if (name.empty())
                        {
                            layout_manager_load_layout(name);
                        }
                    }
                }
            }
        }

    }
    catch (std::exception& ex)
    {
        UNUSED(ex);
        LOG(DBG, "Failed to read settings: ");
        LOG(DBG, ex.what());
    }
}

void layout_manager_save()
{
    // Always save the 'current' layout
    layout_manager_save_layout("", ImGui::SaveIniSettingsToMemory());
}

void layout_manager_save_layouts_file()
{
    toml::table values;
    
    toml::array entries;

    // Store all the layouts with enable keys
    for (auto& [layoutName, layout] : LayoutData.layouts)
    {
        toml::table entry;
        entry.insert("name", layoutName);

        // Encode the binary window information as base 64, so we can reload it for ImGui
        auto enc = cppcodec::base64_rfc4648::encode(layout.windowLayout);
        entry.insert("windows", enc);
      
        toml::table enables;
        for (auto& [key, visible] : layout.showFlags)
        {
            enables.insert(key, visible);
        }
        entry.insert("enables", enables);

        entries.push_back(entry);
    }

    // [layout.entries]
    values.insert("layout", entries);

    std::ofstream fileOut(LayoutData.layoutSettingsPath, std::ios_base::out | std::ios_base::trunc);
    if (fileOut.is_open())
    {
        fileOut << values;
    }
}

void layout_manager_save_layout(const std::string& layoutName, const std::string& layoutString)
{
    // Copy existing show state
    for (auto& [key, value] : LayoutData.mapWindowState)
    {
        LayoutData.layouts[layoutName].showFlags[key] = *value.pVisible;
    }

    // Copy layout state
    LayoutData.layouts[layoutName].windowLayout = layoutString;

    layout_manager_save_layouts_file();
}

void layout_manager_load_layout(const std::string& layoutName)
{
    // Find the layout, if not return
    auto itr = LayoutData.layouts.find(layoutName);
    if (itr == LayoutData.layouts.end())
    {
        return;
    }

    // Set the window show states for things we found; everything else stays the same
    for (auto& [key, value] : itr->second.showFlags)
    {
        auto itrFound = LayoutData.mapWindowState.find(key);
        if (itrFound != LayoutData.mapWindowState.end())
        {
            *itrFound->second.pVisible = value;
        }
    }
    LayoutData.loadCB(layoutName, itr->second);
}

void layout_manager_register_window(const std::string& key, const std::string& name, bool* showState)
{
    // Remember a show pointer for managing window show state
    LayoutData.mapWindowState[key] = WindowState{ name, showState };
}

void layout_manager_do_menu()
{
    {
        if (ImGui::MenuItem("Restore Default"))
        {
            LayoutData.pendingLayoutLoad = "Default";
        }

        if (ImGui::BeginMenu("Load Layout"))
        {
            for (auto& [name, layout] : LayoutData.layouts)
            {
                if (!name.empty())
                {
                    if (ImGui::MenuItem(name.c_str()))
                    {
                        LayoutData.pendingLayoutLoad = name;
                    }
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Save Layout"))
        {
            LayoutData.popupLayoutSaveRequest = true;
        }
#ifdef ___DEBUG
        if (ImGui::MenuItem("Save Layout File (DEBUG)"))
        {
            igfd::ImGuiFileDialog::Instance()->OpenDialog("SaveLayoutFileKey", "Choose File", ".ini", (runtree_path() / "settings").string(), "layout.ini");
        }
#endif

        ImGui::Separator();

        for (auto& [key, state] : LayoutData.mapWindowState)
        {
            ImGui::MenuItem(state.name.c_str(), nullptr, state.pVisible);
        }
    }
}

bool layout_manager_do_menu_popups()
{
    if (LayoutData.popupLayoutSaveRequest == true)
    {
        ImGui::OpenPopup("LayoutName");
        LayoutData.popupLayoutSaveRequest = false;
    }

    if (ImGui::BeginPopupModal("LayoutName", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        char chID[64];
        chID[0] = 0;
        if (ImGui::InputText("Name", chID, 64, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            layout_manager_save_layout(chID, ImGui::SaveIniSettingsToMemory());
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
        return true;
    }
    return false;
}

// We call this outside of ImGui NewFrame to load any pending layouts
void layout_manager_update()
{
    if (!LayoutData.pendingLayoutLoad.empty())
    {
        layout_manager_load_layout(LayoutData.pendingLayoutLoad);
        LayoutData.pendingLayoutLoad.clear();
    }
}

} // namespace MUtils
