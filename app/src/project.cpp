#include <set>
#include <fmt/format.h>

#include <app/project.h>

#include <vklive/file/file.h>
#include <vklive/string/string_utils.h>
#include <vklive/file/runtree.h>
#include <vklive/logger/logger.h>
#include <vklive/scene.h>
#include <vklive/model.h>
#include <vklive/IDevice.h>

#include <app/config.h>

IDevice* GetDevice();

bool project_scene_valid(Project* pProject)
{
    if (!pProject || !pProject->spScene || !pProject->spScene->valid)
    {
        return false; 
    }

    return true;
}

bool project_has_scene(Project* pProject)
{
    if (!pProject || !pProject->spScene)
    {
        return false; 
    }

    return true;
}

void clear_temporary_files()
{
    fs::remove_all(fs::temp_directory_path() / "vk_live" / "temp_projects");
}

void project_startup()
{
    clear_temporary_files();
}
    
fs::path find_temp_path()
{
    auto pathTemp = fs::temp_directory_path() / "vk_live" / "temp_projects";

    // Find a suitable temp folder
    uint32_t count = 0;
    std::string tempDir = "project_0";
    while (fs::exists(pathTemp / tempDir))
    {
        count++;
        tempDir = "project_" + std::to_string(count);
    }
    return pathTemp / tempDir;
}

std::shared_ptr<Project> project_load_to_temp(const fs::path& projectPath)
{
    std::shared_ptr<Project> spProject = std::make_shared<Project>();

    auto pathTemp = find_temp_path();

    try
    {
        // Copy the default project
        file_folder_copy(projectPath, pathTemp);
    } 
    catch(std::exception& ex)
    {
        LOG(ERROR, "Failed project copy: " << ex.what());
    }
    spProject->rootPath = fs::canonical(pathTemp);
    spProject->temporary = true;
    return spProject;
}

std::shared_ptr<Project> project_load(const fs::path& projectPath)
{
    std::shared_ptr<Project> spProject = std::make_shared<Project>();
    if (fs::exists(projectPath) && fs::is_directory(projectPath))
    {
        LOG(DBG, "Loading project: " << projectPath.string());
        // Found a non-temp project
        spProject->rootPath = fs::canonical(projectPath);
        spProject->temporary = false;         
        return spProject;
    }
  
    LOG(DBG, "Loading temp project, since not found: " << projectPath.string());
    return project_load_to_temp(runtree_find_path("projects/default"));
}

std::set<std::string> project_file_extensions()
{
    auto ext = model_file_extensions();
    if (GetDevice())
    {
        auto shaderExt = GetDevice()->ShaderFileExtensions();
        ext.insert(shaderExt.begin(), shaderExt.end());
    }
    ext.insert(".scenegraph");
    ext.insert(".toml");
    return ext;
}

// Copy all the relevent files from a project
// This is more complex than it might first appear.
// 1. Ask user for overwrite
// 2. Only copy known file extension types, and files referenced in the scene
// We don't want to do a deep copy of an erroneously selected project path. It is possible to load
// a project from the wrong path, then try to save it, massively duplicating folder trees....
// So we only copy the known bits.
bool project_copy(Project& project, const fs::path& destPath)
{
    if (!project.spScene || !fs::is_directory(project.rootPath))
    {
        return false; 
    }
    std::set<fs::path> sourcePaths;

    bool ok = true;
    auto root = fs::canonical(project.rootPath);
    try
    {
        auto extensions = project_file_extensions();

        // Walk the source files
        auto files = file_gather_files(project.rootPath);
        for (auto& file : files)
        {
            auto ext = string_tolower(file.extension().string());
            auto itrFound = extensions.find(ext);
            if (itrFound != extensions.end())
            {
                // Find the relevent ones
                sourcePaths.insert(file);
            }
        }

        for (auto& shader : project.spScene->shaders)
        {
            auto p = root / shader.first;
            if (fs::exists(p))
            {
                sourcePaths.insert(fs::canonical(p));
            }
        }
        
        for (auto& geom : project.spScene->models)
        {
            auto p = root / geom.first;
            if (fs::exists(p))
            {
                sourcePaths.insert(fs::canonical(p));
            }
        }

        for (auto& source : sourcePaths)
        {
            auto dest = destPath / file_get_relative_path(root, source);
            LOG(DBG, fmt::format("Writing: {}", dest.string()));
            try
            {
                fs::copy_file(source, dest, fs::copy_options::overwrite_existing);
            }
            catch(std::exception& ex)
            {
                // Something we can't copy?
                LOG(DBG, "Error copying: " << ex.what());
                ok = false;
            }
        }
    }
    catch (std::exception& ex)
    {
        LOG(DBG, ex.what());
        return false;
    }
    return ok;
}

