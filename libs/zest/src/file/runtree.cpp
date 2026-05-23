#include <cassert>
#include "zest/logger/logger.h"

#include "zest/file/runtree.h"
#include "zest/file/file.h"

namespace Zest
{
fs::path runtreePath;

void runtree_init(const char* pszAppPath, const char* pszBuildPath)
{
    auto appPath = fs::path(pszAppPath) / "run_tree";
    if (fs::exists(appPath))
    {
        runtreePath = fs::canonical(appPath);
    }
    else
    {
        appPath = fs::path(pszBuildPath) / "run_tree";
        if (fs::exists(appPath))
        {
            runtreePath = fs::canonical(appPath);
        }
        else
        {
            assert(!"Not found!");
        }
    }
    LOG(DBG, "runtree Path: " << runtreePath.string());
}

void runtree_destroy()
{
    runtreePath.clear();
}

fs::path runtree_find_asset_internal(const fs::path& searchPath)
{
    fs::path found(runtreePath / searchPath);
    if (fs::exists(found))
    {
        //LOG(DEBUG) << "Found file: " << found.string();
        return fs::canonical(fs::absolute(found));
    }

    LOG(DBG, "** File not found: " << searchPath.string());
    return fs::path();
}

fs::path runtree_find_path(const fs::path& p)
{
    return runtree_find_asset_internal(p);
}

std::string runtree_load_asset(const fs::path& p)
{
    auto path = runtree_find_path(p);
    return file_read(path);
}

fs::path runtree_path()
{
    return runtreePath;
}


} // Zest
