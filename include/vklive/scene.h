#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "file/file.h"
#include <vklive/camera.h>
#include <vklive/message.h>

#include "pass.h"

struct Project;
struct IDeviceSurface;

enum class Format
{
    Default,
    Default_Depth,
    R8G8B8A8UNorm,
    D32
};

struct Surface
{
    explicit Surface(const std::string& n)
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

    glm::uvec2 currentSize = glm::uvec2(0);
    uint32_t renderCount = 0;
};

template<>
struct std::hash<Surface>
{
    std::size_t operator()(const Surface& surface) const noexcept
    {
        std::size_t h = std::hash<glm::vec2>()(surface.scale) ^ std::hash<glm::uvec2>()(surface.size);
        return h;
    }
};

enum class GeometryType
{
    Model,
    Rect
};

struct Geometry
{
    Geometry(const fs::path& p)
        : path(p), type(GeometryType::Model)
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
    Shader(const fs::path &n)
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

struct SceneGraph
{
    SceneGraph(const fs::path& p)
        : root(p)
    {
    }

    fs::path root;

    // The source file for this graph
    fs::path sceneGraphPath;

    // Global objects
    std::map<std::string, std::shared_ptr<Surface>> surfaces;
    std::map<fs::path, std::shared_ptr<Geometry>> geometries;
    std::map<fs::path, std::shared_ptr<Shader>> shaders;
    std::map<std::string, std::shared_ptr<Pass>> passes;

    // Initial create order of the passes
    std::vector<Pass*> passOrder;
    std::vector<Pass*> sortedPasses;

    // Generated errors for this graph
    std::vector<Message> errors; 
    std::vector<Message> warnings; 
    std::vector<fs::path> headers;

    bool valid = true;
};

std::shared_ptr<SceneGraph> scenegraph_build(const fs::path& root);
void scenegraph_destroy_parser();
void scenegraph_report_error(SceneGraph& scene, const std::string& txt);

// Graph
void scenegraph_build(SceneGraph& scene);
IDeviceSurface* scenegraph_render(SceneGraph& scene, const glm::vec2& size);


// TODO: Move this
bool format_is_depth(const Format& fmt);

