#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "file/file.h"
#include <vklive/camera.h>
#include <vklive/message.h>

#include "pass.h"

struct Project;

enum class Format
{
    Default,
    Default_Depth,
    R8G8B8A8UNorm,
    D32
};

struct Surface
{
    Surface(const std::string& n)
        : name(n)
    {
    }

    std::string name;

    // Meaning full size framebuffer
    glm::uvec2 size = glm::uvec2(0);

    // 1x
    glm::vec2 scale = glm::vec2(1.0f);

    // Texture file
    fs::path path;

    // Format if a target
    Format format = Format::Default;

    // Has this image been rendered to during the frame?
    bool rendered = false;
};

enum class GeometryType
{
    Model,
    Rect
};

struct Geometry
{
    Geometry(const fs::path& p)
        : path(p)
        , type(GeometryType::Model)
    {
    }

    Geometry(GeometryType t)
        : type(t)
    {
    }

    fs::path path;
    glm::mat4 transform = glm::mat4(1.0f);
    glm::vec3 loadScale = glm::vec3(1.0f);
    GeometryType type = GeometryType::Rect;
};

struct Shader
{
    Shader(const fs::path& n)
        : path(n)
    {
    }

    fs::path path;
};

struct Pass
{
    Pass(const std::string& n)
        : name(n)
    {
    }

    bool hasClear = false;
    glm::vec4 clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    std::string name;
    std::vector<std::string> targets;
    std::vector<std::string> samplers;
    std::string depth;
    std::vector<fs::path> geometries;
    std::vector<fs::path> shaders;

    Camera camera;

    int scriptTargetsLine = 0;
    int scriptSamplersLine = 0;
};

struct Scene
{
    Scene(const fs::path& p)
        : root(p)
    {
    }

    fs::path root;
    fs::path sceneGraphPath;

    // Global objects
    std::map<std::string, std::shared_ptr<Surface>> surfaces;
    std::map<fs::path, std::shared_ptr<Geometry>> geometries;
    std::map<fs::path, std::shared_ptr<Shader>> shaders;
    std::map<std::string, std::shared_ptr<Pass>> passes;
    std::vector<Message> errors;
    std::vector<Message> warnings;
    std::vector<fs::path> headers;

    // Evaluated values for rendering
    std::vector<Pass*> passOrder;
    Surface* finalColorTarget;

    bool valid = true;
};

enum class AssetType
{
    None,
    Texture,
    Model
};

std::shared_ptr<Scene> scene_build(const fs::path& root);
void scene_destroy_parser();
bool format_is_depth(const Format& fmt);
void scene_report_error(Scene& scene, const std::string& txt, const fs::path& path = fs::path(), int32_t line = -1, const std::pair<int32_t, int32_t>& range = std::make_pair(-1, -1));
fs::path scene_find_asset(Scene& scene, const fs::path& path, AssetType assetType = AssetType::None);
