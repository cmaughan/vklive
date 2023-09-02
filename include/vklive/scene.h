#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <zest/file/file.h>

#include <vklive/camera.h>
#include <vklive/message.h>

#include "pass.h"

struct Project;

enum class Format
{
    default_format,
    default_depth_format,
    r8g8b8a8_unorm,
    r16g16b16a16_sfloat,
    d32
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
    Format format = Format::default_format;

    // Has this image been rendered to during the frame?
    bool rendered = false;

    // Is this declared a target?
    bool isTarget = false;
    
    // Is this a ray trace target?
    bool isRayTarget = false;
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

    Geometry(const fs::path& p, GeometryType t)
        : path(p),
        type(t)
    {
    }

    fs::path path;
    glm::mat4 transform = glm::mat4(1.0f);
    glm::vec3 loadScale = glm::vec3(1.0f);
    GeometryType type = GeometryType::Model;
    bool buildAS = false;
};

struct Shader
{
    Shader(const fs::path& n)
        : path(n)
    {
    }

    fs::path path;
};

enum class ShaderType
{
    Vertex,
    Geometry,
    Fragment,
    RayGroupGeneral,
    RayGroupTriangles,
    RayGroupProcedural
};

enum class RayShaderType
{
    Ray_Gen,
    Closest_Hit,
    Any_Hit,
    Miss,
    Callable,
    Intersection
};

struct ShaderGroup
{
    ShaderGroup(ShaderType type)
        : groupType(type)
    {

    }
    ShaderType groupType;
    std::vector<std::pair<RayShaderType, std::shared_ptr<Shader>>> shaders;
};

struct Scene;
struct PassSampler
{
    std::string sampler;
    bool sampleAlternate = false;
};

enum class PassType
{
    Unknown,
    Standard,
    RayTracing
};

struct Pass
{
    Pass(Scene& s, const std::string& n)
        : name(n),
          scene(s)
    {
    }

    Scene& scene;
    bool hasClear = false;
    PassType passType = PassType::Unknown;
    glm::vec4 clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    std::string name;
    std::vector<std::string> targets;
    std::vector<PassSampler> samplers;
    std::string depth;
    std::vector<fs::path> models;
    std::vector<fs::path> shaders;
    std::vector<std::string> cameras;
    std::vector<std::shared_ptr<ShaderGroup>> shaderGroups;

    int scriptTargetsLine = 0;
    int scriptSamplersLine = 0;
    int scriptPassLine = 0;
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
    std::map<std::string, std::shared_ptr<Camera>> cameras;
    std::map<fs::path, std::shared_ptr<Geometry>> models;
    std::map<fs::path, std::shared_ptr<Shader>> shaders;
    std::map<std::string, std::shared_ptr<Pass>> passes;
    std::vector<Message> errors;
    std::vector<Message> warnings;
    std::vector<fs::path> headers;

    // Evaluated values for rendering
    std::vector<Pass*> passOrder;
    Surface* finalColorTarget;

    bool valid = true;

    static uint64_t GlobalFrameCount;
    static double GlobalElapsedSeconds;
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
void scene_report_error(Scene& scene, MessageSeverity severity, const std::string& txt, const fs::path& path = fs::path(), int32_t line = -1, const std::pair<int32_t, int32_t>& range = std::make_pair(-1, -1));
fs::path scene_find_asset(Scene& scene, const fs::path& path, AssetType assetType = AssetType::None);
Surface* scene_get_surface(Scene& scene, const std::string& surfacename);
Camera* scene_get_camera(Scene& scene, const std::string& cameraName);
void scene_copy_state(Scene& dest, Scene& source);

bool scene_is_raytracer(const fs::path& path);
bool scene_is_shader(const fs::path& path);
bool scene_is_edit_file(const fs::path& path);
bool scene_is_header(const fs::path& path);
bool scene_is_scenegraph(const fs::path& path);

