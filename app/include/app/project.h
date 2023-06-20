#pragma once

#include <set>
#include <vector>

#include <zest/file/file.h>

#include <vklive/message.h>

struct Scene;

struct Project
{
    fs::path rootPath;
    std::shared_ptr<Scene> spScene;
    std::vector<Message> projectMessages;
    bool temporary = false;
    bool modified = false;
};

void project_startup();
std::shared_ptr<Project> project_load(const fs::path& path);
std::shared_ptr<Project> project_load_to_temp(const fs::path& path);
bool project_scene_valid(Project* pProject);
bool project_has_scene(Project* pProject);
std::set<std::string> project_file_extensions();
bool project_copy(Project& project, const fs::path& destPath, std::string& error);