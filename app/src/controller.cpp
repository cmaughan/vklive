#include <tinyfiledialogs/tinyfiledialogs.h>
#include <nfd.h>
#include <fmt/format.h>

#include <vklive/scene.h>
#include <vklive/logger/logger.h>
#include <vklive/string/string_utils.h>

#include <app/controller.h>
#include <app/config.h>

#include "config_app.h"

// tinyfd
enum class BoxRet
{
    No = 0,
    Yes = 1,
}; /* 0 for cancel/no , 1 for ok/yes , 2 for no in yesnocancel */

void controller_load_project(const fs::path& projectPath)
{
    auto project = project_load(projectPath);
    if (project)
    {
        g_Controller.spProjectQueue->enqueue(project);
    }
}

fs::path controller_open_project()
{
    nfdchar_t *pszPath = NULL;

    auto docPath = (file_documents_path() / "VkLive");
    if (!appConfig.last_folder_path.empty() && fs::exists(appConfig.last_folder_path) && fs::is_directory(appConfig.last_folder_path))
    {
        docPath = appConfig.last_folder_path;
    }
    else
    {
        appConfig.last_folder_path = fs::path(); 
    }

    NFD_PickFolder(docPath.string().c_str(), &pszPath);
    if (pszPath)
    {
        fs::path p(pszPath);
        if (fs::exists(p) && fs::is_directory(p))
        {
            if (fs::exists(p.parent_path()))
            {
                appConfig.last_folder_path = p.parent_path();
            }
            return p;
        }
    }
    return fs::path();
}

fs::path controller_save_project_as()
{
    // Can only copy existing folder/project
    if (!g_Controller.spCurrentProject || 
        !fs::is_directory(g_Controller.spCurrentProject->rootPath))
    {
        return fs::path(); 
    }
    auto docPath = (file_documents_path() / "VkLive");

    fs::create_directories(docPath);
    nfdchar_t *pszPath = NULL;
    NFD_PickFolder(docPath.string().c_str(), &pszPath);
    if (pszPath)
    {
        auto p = fs::path(pszPath);
        if (fs::equivalent(p, g_Controller.spCurrentProject->rootPath) || !fs::is_directory(p))
        {
            // Already there.  empty path to say we didn't save
            LOG(DBG, "Not saving project to same folder / or doesn't exist");
            return fs::path();
        }

        // Stop the user overwriting
        auto existing = file_gather_files(p);
        if (!existing.empty())
        {
            auto ret = (BoxRet)tinyfd_messageBox("Overwrite?", "The target folder is not empty\ndo you wish to overwrite it?", "yesno", "warning", (int)BoxRet::No);
            if (ret == BoxRet::No)
            {
                return fs::path();
            }
        }

        LOG(DBG, "Copying project from: " + g_Controller.spCurrentProject->rootPath.string() + " to: " << p.string());
        std::string error;
        if (!project_copy(*g_Controller.spCurrentProject, p, error))
        {
            // tfd doesn't like quotes inside messages
            string_replace_in_place(error, "\"", "");
            string_replace_in_place(error, "'", "");
            tinyfd_messageBox("Error", fmt::format("The project could not be copied!\n{}", error).c_str(), "ok", "error", (int)BoxRet::No);
            return fs::path(); 
        }
        return p;
    }
    LOG(DBG, "Path not chosen");
    return fs::path();
}

bool controller_check_exit()
{
    if (!g_Controller.spCurrentProject)
    {
        return true; 
    }

    if (!g_Controller.spCurrentProject->temporary)
    {
        return true;
    }

    // Temporary project was not modified
    if (!g_Controller.spCurrentProject->modified)
    {
        return true; 
    }

    try
    {
        auto ret = (BoxRet)tinyfd_messageBox("Save Current Project", "The current project is in a temporary folder,\ndo you wish to save it?", "yesno", "warning", (int)BoxRet::Yes);
        if (ret == BoxRet::No)
        {
            return true;
        }
        else if (ret == BoxRet::Yes)
        {
            auto docPath = (file_documents_path() / "VkLive");
            fs::create_directories(docPath);

            auto newPath = controller_save_project_as();
            if (!newPath.empty() && fs::exists(newPath))
            {
                // Force the update of the config, and remove the project so we ensure that the correct path is saved on exit!
                appConfig.project_root = newPath;
                g_Controller.spCurrentProject.reset();
                return true;
            }

            return false;
        }
    }
    catch (std::exception& ex)
    {
        // An error, keep running
        return false;
    }
    return false;
}
