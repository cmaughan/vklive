#pragma once

#include <concurrentqueue/concurrentqueue.h>
#include <vklive/file/file.h>
#include <app/project.h>

struct Controller
{
    std::shared_ptr<Project> spCurrentProject;
    std::shared_ptr<moodycamel::ConcurrentQueue<std::shared_ptr<Project>>> spProjectQueue;
};

extern struct Controller g_Controller;

bool controller_check_exit();
fs::path controller_save_project_as();
fs::path controller_open_project();
