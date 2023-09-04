#include <algorithm>
#include <fmt/format.h>
#include <iostream>
#include <sstream>

#include "toml++/toml.h"

#include <zest/file/file.h>
#include <zest/file/runtree.h>
#include <zest/logger/logger.h>
#include <zest/string/string_utils.h>

#include <vklive/scene.h>

#include "config_app.h"
    
uint64_t Scene::GlobalFrameCount = 0;
double Scene::GlobalElapsedSeconds = 0.0;

extern "C" {
#include "mpc/mpc.h"
}

#define T_CLEAR "clear"
#define T_COMMENT "comment"
#define T_DISABLE "disable"
#define T_FLOAT "float"
#define T_FORMAT "format"
#define T_FS "fs"
#define T_GEOMETRY "geometry"
#define T_GS "gs"
#define T_IDENT "ident"
#define T_IDENT_ARRAY "ident_array"
#define T_PASS "pass"
#define T_PATH "path"
#define T_PATH_NAME "path_name"
#define T_BUILD_AS "build_as"
#define T_SAMPLERS "samplers"
#define T_SCALE "scale"
#define T_SCENEGRAPH "scenegraph"
#define T_SIZE "size"
#define T_SURFACE "surface"
#define T_CAMERA "camera"
#define T_CAMERA_ID "camera_id"
#define T_POSITION "position"
#define T_LOOK_AT "look_at"
#define T_FIELD_OF_VIEW "field_of_view"
#define T_NEAR_FAR "near_far"
#define T_TARGETS "targets"
#define T_VECTOR "vector"
#define T_BOOL "bool"
#define T_VS "vs"
#define T_RAY_GROUP_GENERAL "ray_group_general"
#define T_RAY_GROUP_TRIANGLES "ray_group_triangles"
#define T_RAY_GROUP_PROCEDURAL "ray_group_procedural"
#define T_RAY_GEN "ray_gen"
#define T_RAY_ANY_HIT "any_hit"
#define T_RAY_MISS "miss"
#define T_RAY_CLOSEST_HIT "closest_hit"
#define T_RAY_INTERSECTION "intersection"
#define T_RAY_CALLABLE "callable"

#include <concurrentqueue/concurrentqueue.h>

struct Parser
{
    mpc_parser_t* pSceneGraph = nullptr;
    std::vector<mpc_parser_t*> parsers;
    mpc_err_t* pError = nullptr;
};
Parser parser;

const auto ShaderTypes = std::map<std::string, ShaderType>{
    { T_VS, ShaderType::Vertex },
    { T_GS, ShaderType::Geometry },
    { T_FS, ShaderType::Fragment },
    { T_RAY_GROUP_GENERAL, ShaderType::RayGroupGeneral },
    { T_RAY_GROUP_TRIANGLES, ShaderType::RayGroupTriangles },
    { T_RAY_GROUP_PROCEDURAL, ShaderType::RayGroupProcedural },
};

const auto RayShaderTypes = std::map<std::string, RayShaderType>{
    { T_RAY_GEN, RayShaderType::Ray_Gen },
    { T_RAY_CLOSEST_HIT, RayShaderType::Closest_Hit },
    { T_RAY_INTERSECTION, RayShaderType::Intersection },
    { T_RAY_CALLABLE, RayShaderType::Callable },
    { T_RAY_ANY_HIT, RayShaderType::Any_Hit },
    { T_RAY_MISS, RayShaderType::Miss }
};

const auto Formats = std::map<std::string, Format>{
    { "default_format", Format::default_format },
    { "default_color_format", Format::default_format },
    { "default_color", Format::default_format },
    { "color_format", Format::default_format },
    { "r8g8b8a8_unorm", Format::r8g8b8a8_unorm },
    { "rgba8", Format::r8g8b8a8_unorm },

    { "r16g16b16a16_sfloat", Format::r16g16b16a16_sfloat },
    { "rgba16f", Format::r16g16b16a16_sfloat },

    { "d32", Format::d32 },
    { "default_depth_format", Format::default_depth_format },
    { "depth_format", Format::default_depth_format },
    { "default_depth", Format::default_depth_format }
};

bool format_is_depth(const Format& fmt)
{
    switch (fmt)
    {
    case Format::default_depth_format:
    case Format::d32:
        return true;
    default:
        return false;
    }
}

std::string sanitize_mpc_error(mpc_err_t* pErr)
{
    auto errString = std::string(mpc_err_string(pErr));
    auto startPos = errString.find("error:");
    if (startPos != std::string::npos)
    {
        errString = errString.substr(startPos);
    }
    return Zest::string_trim(errString);
}

void scene_init_parser()
{
    if (parser.pError || parser.pSceneGraph)
    {
        return;
    }
    // We have a very simple .scenegraph file format.
    // As always, when I have something like this, orangeduck/mpc
    // is the easy way to parse it, if I don't want to use lisp or LLVM.

#define ADD_PARSER(var, tag)          \
    mpc_parser_t* var = mpc_new(tag); \
    parser.parsers.push_back(var)

    ADD_PARSER(clear, T_CLEAR);
    ADD_PARSER(comment, T_COMMENT);
    ADD_PARSER(disable, T_DISABLE);
    ADD_PARSER(flt, T_FLOAT);
    ADD_PARSER(format, T_FORMAT);
    ADD_PARSER(ident, T_IDENT);
    ADD_PARSER(ident_array, T_IDENT_ARRAY);

    ADD_PARSER(pass, T_PASS);
    ADD_PARSER(samplers, T_SAMPLERS);
    ADD_PARSER(targets, T_TARGETS);
    ADD_PARSER(fs, T_FS);
    ADD_PARSER(gs, T_GS);
    ADD_PARSER(vs, T_VS);
    ADD_PARSER(geometry, T_GEOMETRY);

    // RT
    ADD_PARSER(ray_group_general, T_RAY_GROUP_GENERAL);
    ADD_PARSER(ray_group_triangles, T_RAY_GROUP_TRIANGLES);
    ADD_PARSER(ray_group_procedural, T_RAY_GROUP_PROCEDURAL);
    ADD_PARSER(ray_gen, T_RAY_GEN);
    ADD_PARSER(miss, T_RAY_MISS);
    ADD_PARSER(any_hit, T_RAY_ANY_HIT);
    ADD_PARSER(closest_hit, T_RAY_CLOSEST_HIT);
    ADD_PARSER(intersection, T_RAY_INTERSECTION);
    ADD_PARSER(callable, T_RAY_CALLABLE);


    ADD_PARSER(path_id, T_PATH);
    ADD_PARSER(build_as, T_BUILD_AS);
    ADD_PARSER(path_name, T_PATH_NAME);
    ADD_PARSER(scale, T_SCALE);
    ADD_PARSER(size, T_SIZE);
    ADD_PARSER(surface, T_SURFACE);

    ADD_PARSER(camera, T_CAMERA);
    ADD_PARSER(camera_id, T_CAMERA_ID);
    ADD_PARSER(position, T_POSITION);
    ADD_PARSER(look_at, T_LOOK_AT);
    ADD_PARSER(near_far, T_NEAR_FAR);
    ADD_PARSER(field_of_view, T_FIELD_OF_VIEW);

    ADD_PARSER(bool_id, T_BOOL);
    ADD_PARSER(vector, T_VECTOR);

    // Special case; we hold onto it.
    parser.pSceneGraph = mpc_new(T_SCENEGRAPH);
    parser.parsers.push_back(parser.pSceneGraph);

    parser.pError = mpca_lang(MPCA_LANG_DEFAULT, R"(
path_name        : /[a-zA-Z_\-][a-zA-Z0-9_\-\/.]*/ ;
path             : "path" ":" <path_name> ;
comment          : /\/\/[^\n\r]*/ ;
ident            : /[!]?[a-zA-Z_][a-zA-Z0-9_-]*/ ;
float            : /[+-]?\d+(\.\d+)?([eE][+-]?[0-9]+)?/ ;
bool             : "true" | "false" ;
vector           : ('(' <float> (','? <float>)? (','? <float>)? (','? <float>)? ')') | <float> ;
ident_array      : ('(' <ident> (','? <ident>)? (','? <ident>)? (','? <ident>)? (','? <ident>)? ')') | <ident> ;
build_as         : "build_as" ":" <bool> ;
scale            : "scale" ':' <vector> ;
size             : "size" ':' <vector> ;
clear            : "clear" ':' <vector> ;
format           : "format" ':' <ident> ;
samplers         : "samplers" ':' <ident_array> ;
targets          : "targets" ':' <ident_array> ;
camera_id        : "camera" ':' <ident_array> ;
look_at          : "look_at" ':' <vector> ;
position         : "position" ':' <vector> ;
near_far         : "near_far" ':' <vector> ;
field_of_view    : "field_of_view" ':' <float> ;
vs               : "vs" ':' <path_name> ;
gs               : "gs" ':' <path_name> ;
fs               : "fs" ':' <path_name> ;
ray_gen          : "ray_gen" ':' <path_name> ;
miss             : "miss" ':' <path_name> ;
callable         : "callable" ':' <path_name> ;
closest_hit      : "closest_hit" ':' <path_name> ;
any_hit          : "any_hit" ':' <path_name> ;
intersection     : "intersection" ':' <path_name> ;
surface          : "surface" ':' <ident> '{' (<comment> | <path> | <clear> | <format> | <scale> | <size>)* '}';
camera           : "camera" ':' <ident> '{' (<comment> | <position> | <look_at> | <field_of_view> | <near_far>)* '}';
ray_group_general : "ray_group_general" ':' <ident> '{' (<ray_gen> | <miss> | <callable>) '}';
ray_group_triangles : "ray_group_triangles" ':' <ident> '{' (<closest_hit> | <any_hit>)* '}';
ray_group_procedural : "ray_group_procedural" ':' <ident> '{' <intersection> (<closest_hit> | <any_hit>)* '}';
geometry         : "geometry" ':' <ident> '{' (<path> | <scale> | <build_as> | <ray_group_general> | <ray_group_triangles> | <ray_group_procedural> | <vs> | <fs> | <gs> | <comment>)* '}';
disable          : '!' ;
pass             : <disable>? "pass" ':' <ident> '{' (<geometry> | <targets> | <samplers> | <camera_id> | <comment> | <clear>)* '}'; 
scenegraph       : /^/ (<comment> | <surface> | <camera>)* (<comment> | <pass> )* /$/ ;
    )",
        path_name, path_id, comment, ident, bool_id, flt, vector, ident_array, build_as, scale, size, clear, format,
        samplers, targets, vs, gs, fs, surface, camera, camera_id, position, look_at, field_of_view, near_far, geometry, disable, pass, ray_group_general, ray_group_triangles, ray_group_procedural, ray_gen, miss, any_hit, closest_hit, intersection, callable, parser.pSceneGraph, nullptr);
}

void scene_destroy_parser()
{
    if (parser.pError)
    {
        mpc_err_delete(parser.pError);
    }

    for (auto& p : parser.parsers)
    {
        mpc_cleanup(1, p);
    }
    parser.parsers.clear();
    parser.pError = nullptr;
    parser.pSceneGraph = nullptr;
}

// Find the scene graph path
fs::path scene_get_scenegraph(const fs::path& root, const std::vector<fs::path>& files)
{
    if (fs::is_directory(root))
    {
        auto projectFile = fs::path(root / "project.toml");
        if (fs::exists(projectFile))
        {
            try
            {
                toml::table tbl = toml::parse_file(projectFile.string());
                LOG(INFO, "Project.toml:\n"
                        << tbl);

                auto sceneGraph = tbl["settings"]["scenegraph"].value_or("");
                auto p = root / sceneGraph;
                if (fs::exists(p) && fs::is_regular_file(p))
                {
                    return fs::canonical(p);
                }
            }
            catch (std::exception& ex)
            {
                LOG(DBG, "No valid project file");
            }
        }
    }

    fs::path sceneGraphPath;
    for (auto& file : files)
    {
        if (file.extension() == ".scenegraph")
        {
            sceneGraphPath = file;
        }
    }

    // If there is no scenegraph, make an empty one
    if (!fs::exists(sceneGraphPath) && fs::is_directory(root))
    {
        sceneGraphPath = root / "scene.scenegraph";
        try
        {
            Zest::file_write(sceneGraphPath, "# Scenegraph");
        }
        catch (std::exception& ex)
        {
        }
    }
    return sceneGraphPath;
}

Surface* scene_get_surface(Scene& scene, const std::string& surfaceName)
{
    auto itr = scene.surfaces.find(surfaceName);
    if (itr == scene.surfaces.end())
    {
        return nullptr;
    }

    return itr->second.get();
}

Camera* scene_get_camera(Scene& scene, const std::string& cameraName)
{
    auto itr = scene.cameras.find(cameraName);
    if (itr == scene.cameras.end())
    {
        return nullptr;
    }

    return itr->second.get();
}

std::vector<fs::path> scene_get_headers(const std::vector<fs::path>& files)
{
    std::vector<fs::path> headers;
    for (auto& f : files)
    {
        if (f.extension() == ".h" || f.extension() == ".hpp")
        {
            headers.push_back(f);
        }
    }
    return headers;
}

void AddMessage(Scene& scene, const std::string& message, MessageSeverity severity = MessageSeverity::Error, uint32_t lineIndex = 0, int32_t column = -1)
{
    Message msg;
    msg.severity = severity;
    msg.path = scene.sceneGraphPath;
    msg.line = lineIndex;
    if (column != -1)
    {
        msg.range = std::make_pair(column, column + 1);
    }
    msg.text = message;

    switch (severity)
    {
    case MessageSeverity::Warning:
    case MessageSeverity::Message:
        scene.warnings.push_back(msg);
        break;
    default:
    case MessageSeverity::Error:
        scene.errors.push_back(msg);
        scene.valid = false;
        break;
    }

    LOG(DBG, message);
}

// Ensure that samplers have been set up correctly
void validate_samplers(Scene& scene)
{
    for (auto& pass : scene.passes)
    {
        for (auto& passSampler : pass->samplers)
        {
            for (auto& passTarget : pass->targets)
            {
                if (passSampler.sampler == passTarget)
                {
                    if (!passSampler.sampleAlternate)
                    {
                        AddMessage(scene, fmt::format("To sample and write to the same target, use '!' to label the sampler: {}", passSampler.sampler), MessageSeverity::Warning, pass->scriptSamplersLine);
                    }
                    passSampler.sampleAlternate = true;
                }
            }
        }
    }
}

std::shared_ptr<Scene> scene_build(const fs::path& root)
{
    LOG(DBG, "scene_build: " << root.string());

    std::shared_ptr<Scene> spScene = std::make_shared<Scene>(root);

    auto files = Zest::file_gather_files(root);

    spScene->sceneGraphPath = scene_get_scenegraph(root, files);
    spScene->headers = scene_get_headers(files);
    spScene->valid = true;

    scene_init_parser();

    // Add the error to this scene's file
    if (parser.pError != NULL)
    {
        AddMessage(*spScene, sanitize_mpc_error(parser.pError), MessageSeverity::Error, parser.pError->state.row, parser.pError->state.col);
        return spScene;
    }

    // Default backbuffer and depth targets
    auto spDefaultColor = std::make_shared<Surface>("default_color");
    spDefaultColor->format = Format::default_format;
    spDefaultColor->isTarget = true;

    auto spDefaultDepth = std::make_shared<Surface>("default_depth");
    spDefaultDepth->format = Format::default_depth_format;
    spDefaultDepth->isTarget = true;

    auto spDefaultCamera = std::make_shared<Camera>("default_camera");
    spDefaultCamera->nearFar = glm::vec2(0.1f, 256.0f);
    camera_set_pos_lookat(*spDefaultCamera, glm::vec3(0.0f, 0.0f, 4.0f), glm::vec3(0.0f, 0.0f, 0.0f));

    spScene->surfaces["default_color"] = spDefaultColor;
    spScene->surfaces["default_depth"] = spDefaultDepth;
    spScene->cameras["default_camera"] = spDefaultCamera;
    spScene->finalColorTarget = spDefaultColor.get();

    try
    {
        int passStartLine = 0;
        mpc_result_t r;
        if (mpc_parse_contents(spScene->sceneGraphPath.string().c_str(), parser.pSceneGraph, &r))
        {
            auto ast_current = (mpc_ast_t*)r.output;
            // mpc_ast_print((mpc_ast_t*)r.output);

            auto childrenOf = [&](mpc_ast_t* entry, const std::string& val) {
                std::vector<mpc_ast_t*> children;
                if (entry == nullptr)
                {
                    return children;
                }
                for (int i = 0; i < entry->children_num; i++)
                {
                    if (strstr(entry->children[i]->tag, val.c_str()))
                    {
                        children.push_back(entry->children[i]);
                    }
                }
                return children;
            };

            auto hasChild = [&](auto entry, const std::string& val) {
                for (int i = 0; i < entry->children_num; i++)
                {
                    if (strstr(entry->children[i]->tag, val.c_str()))
                    {
                        return true;
                    }
                }
                return false;
            };

            auto getChild = [&](auto entry, const std::string& val) {
                for (int i = 0; i < entry->children_num; i++)
                {
                    if (strstr(entry->children[i]->tag, val.c_str()))
                    {
                        return entry->children[i];
                    }
                }
                std::ostringstream tags;
                for (auto& val : val)
                {
                    tags << val << " ";
                }

                AddMessage(*spScene, std::string("Not found: " + val), MessageSeverity::Error, entry->state.row);
                throw std::domain_error(fmt::format("tag not found {}", tags.str()).c_str());
            };

            auto getBool = [&](auto entry) {
                if (!entry)
                {
                    return false;
                }
                auto pChild = getChild(entry, T_BOOL);
                return std::string(pChild->contents) == "true";
            };

            auto getVector = [&](auto entry, auto& ret, int min, int max) {
                auto pChild = getChild(entry, T_VECTOR);
                auto vals = childrenOf(pChild, T_FLOAT);

                if (vals.size() < min || vals.size() > max)
                {
                    AddMessage(*spScene, fmt::format("Wrong size vector: {}", entry->tag), MessageSeverity::Error, entry->state.row);
                }

                for (int i = 0; i < std::max(ret.length(), std::min(1, int(vals.size()))); i++)
                {
                    ret[i] = std::stof(vals[i]->contents);
                }
                return vals.size();
            };
            
            auto getScalar = [&](auto entry, auto& ret) {
                auto pChild = getChild(entry, T_FLOAT);
                if (!pChild)
                {
                    AddMessage(*spScene, fmt::format("Missing value: {}", entry->tag), MessageSeverity::Error, entry->state.row);
                    return;
                }
                
                ret = std::stof(pChild->contents);
            };

            auto getVectorIdent = [&](auto entry, int min, int max) -> std::vector<std::string> {
                auto pChild = getChild(entry, T_IDENT_ARRAY);
                auto vals = childrenOf(pChild, T_IDENT);

                if (vals.size() < min || vals.size() > max)
                {
                    AddMessage(*spScene, fmt::format("Wrong size vector: {}", entry->tag), MessageSeverity::Error, entry->state.row);
                }

                std::vector<std::string> ret;
                for (auto& v : vals)
                {
                    ret.push_back(v->contents);
                }
                return ret;
            };

            auto getPath = [&](auto entry) {
                auto pPathNameNode = getChild(getChild(entry, T_PATH), T_PATH_NAME);
                return pPathNameNode->contents;
            };

            // LOG(DBG, "Tag: " << ast_current->tag << " Contents: " << ast_current->contents);

            auto cameras = childrenOf(ast_current, T_CAMERA);
            for (auto& pCameraNode : cameras)
            {
                auto pCameraNameNode = getChild(pCameraNode, T_IDENT);

                std::shared_ptr<Camera> spCamera;
                if (pCameraNameNode->contents == "default_camera")
                {
                    spCamera = spDefaultCamera;
                }
                else
                {
                    spCamera = std::make_shared<Camera>(pCameraNameNode->contents);
                }

                glm::vec3 position = glm::vec3(0.0f, 0.0f, 5.0f);
                glm::vec3 look_at = glm::vec3(0.0f);
                glm::vec2 near_far = glm::vec2(0.1f, 256.0f);

                if (hasChild(pCameraNode, T_POSITION))
                {
                    getVector(getChild(pCameraNode, T_POSITION), position, 3, 3);
                }
                if (hasChild(pCameraNode, T_LOOK_AT))
                {
                    getVector(getChild(pCameraNode, T_LOOK_AT), look_at, 3, 3);
                }
                camera_set_pos_lookat(*spCamera, position, look_at);

                if (hasChild(pCameraNode, T_NEAR_FAR))
                {
                    getVector(getChild(pCameraNode, T_NEAR_FAR), near_far, 2, 2);
                }
                camera_set_near_far(*spCamera, near_far);

                if (hasChild(pCameraNode, T_FIELD_OF_VIEW))
                {
                    auto pFOVNode = getChild(pCameraNode, T_FIELD_OF_VIEW);
                    getScalar(pFOVNode, spCamera->fieldOfView);
                }
                spScene->cameras[spCamera->name] = spCamera;
            }

            auto surfaces = childrenOf(ast_current, T_SURFACE);
            for (auto& pSurfaceNode : surfaces)
            {
                auto pSurfaceNameNode = getChild(pSurfaceNode, T_IDENT);

                auto spSurface = std::make_shared<Surface>(pSurfaceNameNode->contents);

                if (hasChild(pSurfaceNode, T_PATH))
                {
                    spSurface->path = getPath(pSurfaceNode);
                }
                if (hasChild(pSurfaceNode, T_SIZE))
                {
                    getVector(getChild(pSurfaceNode, T_SIZE), spSurface->size, 1, 3);
                }
                if (hasChild(pSurfaceNode, T_SCALE))
                {
                    getVector(getChild(pSurfaceNode, T_SCALE), spSurface->scale, 1, 3);
                }

                if (hasChild(pSurfaceNode, T_FORMAT))
                {
                    auto pFormatNode = getChild(pSurfaceNode, T_FORMAT);
                    auto strFormat = getChild(pFormatNode, T_IDENT)->contents;
                    auto strLowerFormat = Zest::string_tolower(strFormat);
                    auto itrFormat = Formats.find(strLowerFormat);
                    if (itrFormat == Formats.end())
                    {
                        AddMessage(*spScene, fmt::format("Format not found: {}", strFormat), MessageSeverity::Error, pFormatNode->state.row, pFormatNode->state.col);
                    }
                    else
                    {
                        spSurface->format = itrFormat->second;
                    }
                }
                spScene->surfaces[spSurface->name] = spSurface;
            }

            auto passes = childrenOf(ast_current, T_PASS);
            for (auto& pPassNode : passes)
            {
                // Find the next pass
                auto pPassNameNode = getChild(pPassNode, T_IDENT);
                if (hasChild(pPassNode, T_DISABLE))
                {
                    continue;
                }
                auto spPass = std::make_shared<Pass>(*spScene, pPassNameNode->contents);

                spPass->scriptPassLine = int(pPassNode->state.row);

                auto setPassType = [&](PassType type, int row = 0) {
                    if ((spPass->passType != PassType::Unknown) && (spPass->passType != type))
                    {
                        AddMessage(*spScene, fmt::format("Pass {} can only be RT or shading?", spPass->name), MessageSeverity::Error, row);
                    }
                    spPass->passType = type;
                };
                auto models = childrenOf(pPassNode, T_GEOMETRY);
                for (auto& pGeometryNode : models)
                {
                    auto pGeomNameNode = getChild(pGeometryNode, T_IDENT);
                    auto pPathNameNode = getChild(getChild(pGeometryNode, T_PATH), T_PATH_NAME);

                    std::shared_ptr<Geometry> spGeom;
                    auto geomPath = fs::path(pPathNameNode->contents);
                    if (geomPath.filename() == "screen_rect")
                    {
                        spGeom = std::make_shared<Geometry>(geomPath, GeometryType::Rect);
                    }
                    else
                    {
                        auto foundPath = scene_find_asset(*spScene, geomPath, AssetType::Model);
                        if (foundPath.empty() || !fs::exists(foundPath))
                        {
                            AddMessage(*spScene, std::string("Geometry missing: " + geomPath.filename().string()), MessageSeverity::Error, pGeometryNode->state.row);
                            continue;
                        }
                        spGeom = std::make_shared<Geometry>(foundPath);
                    }
                    
                    if (hasChild(pGeometryNode, T_BUILD_AS))
                    {
                        spGeom->buildAS = getBool(getChild(pGeometryNode, T_BUILD_AS));
                    }

                    auto addShader = [&](auto pEntry) -> std::shared_ptr<Shader> {
                        auto pPathNode = getChild(pEntry, T_PATH);
                        auto shaderPath = root / pPathNode->contents;
                        if (!fs::exists(shaderPath))
                        {
                            AddMessage(*spScene, std::string("Shader missing: " + shaderPath.filename().string()), MessageSeverity::Error, pPathNode->state.row, pPathNode->state.col);
                            return nullptr;
                        }
                        auto spShaderFrag = std::make_shared<Shader>(shaderPath);
                        spScene->shaders[spShaderFrag->path] = spShaderFrag;
                        spPass->shaders.push_back(shaderPath);
                        return spShaderFrag;
                    };

                    // Shaders
                    for (auto& [ident, type] : ShaderTypes)
                    {
                        auto shaderEntries = childrenOf(pGeometryNode, ident);
                        for (auto& pShaderEntry : shaderEntries)
                        {
                            if (type == ShaderType::Fragment || type == ShaderType::Geometry || type == ShaderType::Vertex)
                            {
                                setPassType(PassType::Standard, pGeometryNode->state.row);

                                addShader(pShaderEntry);
                            }
                            else
                            {
                                setPassType(PassType::RayTracing, pGeometryNode->state.row);

                                auto spShaderGroup = std::make_shared<ShaderGroup>(type);
                                spPass->shaderGroups.push_back(spShaderGroup);
                                for (auto& [ray_ident, ray_type] : RayShaderTypes)
                                {
                                    auto groupEntries = childrenOf(pShaderEntry, ray_ident);
                                    for (auto& pGroupEntry : groupEntries)
                                    {
                                        auto spShader = addShader(pGroupEntry);
                                        if (spShader)
                                        {
                                            spShaderGroup->shaders.push_back(std::make_pair(ray_type, spShader));
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (spPass->shaders.empty())
                    {
                        AddMessage(*spScene, fmt::format("No shaders in geometry: {}", pGeomNameNode->contents), MessageSeverity::Error, pGeomNameNode->state.row);
                        continue;
                    }

                    // Scale
                    auto scaleEntries = childrenOf(pGeometryNode, T_SCALE);
                    for (auto& pScaleNode : scaleEntries)
                    {
                        getVector(pScaleNode, spGeom->loadScale, 1, 3);
                    }

                    spScene->models[spGeom->path] = spGeom;
                    spPass->models.push_back(spGeom->path);
                }

                // Clears
                auto clearEntries = childrenOf(pPassNode, T_CLEAR);
                for (auto& pClearNode : clearEntries)
                {
                    auto pVecNode = getChild(pClearNode, T_VECTOR);
                    auto vals = childrenOf(pVecNode, T_FLOAT);
                    for (int i = 0; i < std::min(3, int(vals.size())); i++)
                    {
                        // Temporarily do it at load time
                        spPass->clearColor[i] = std::stof(vals[i]->contents);
                        spPass->hasClear = true;
                    }
                }

                auto cameraEntries = childrenOf(pPassNode, T_CAMERA_ID);
                for (auto& pCameraIdNode : cameraEntries)
                {
                    auto pCameraName = getChild(pCameraIdNode, T_IDENT);
                    if (pCameraName)
                    {

                        auto itrFound = spScene->cameras.find(pCameraName->contents);
                        if (itrFound == spScene->cameras.end())
                        {
                            AddMessage(*spScene, fmt::format("Camera not found in pass: {}", pCameraName->contents), MessageSeverity::Error, pCameraIdNode->state.row, pCameraIdNode->state.col);
                        }
                        else
                        {
                            spPass->cameras.push_back(pCameraName->contents);
                        }
                    }
                }
                
                if (spPass->cameras.empty())
                {
                    spPass->cameras.push_back("default_camera");
                }

                if (hasChild(pPassNode, T_TARGETS))
                {
                    auto pTargetNode = getChild(pPassNode, T_TARGETS);
                    spPass->targets = getVectorIdent(pTargetNode, 1, 5);
                    for (auto& target : spPass->targets)
                    {
                        auto itrFound = spScene->surfaces.find(target);
                        if (itrFound == spScene->surfaces.end())
                        {
                            AddMessage(*spScene, fmt::format("Surface not found in pass: {}", target), MessageSeverity::Error, pTargetNode->state.row, pTargetNode->state.col);
                        }
                        else
                        {
                            // Remember that we consider this a target
                            itrFound->second->isTarget = true;
                            if (spPass->passType == PassType::RayTracing)
                            {
                                itrFound->second->isRayTarget = true;
                            }
                        }
                    }
                    spPass->scriptTargetsLine = int(pTargetNode->state.row);
                }

                if (hasChild(pPassNode, T_SAMPLERS))
                {
                    auto pSamplerNode = getChild(pPassNode, T_SAMPLERS);
                    auto vecSamplers = getVectorIdent(pSamplerNode, 1, 5);
                    for (auto& sampler : vecSamplers)
                    {
                        PassSampler passSampler{ sampler, false };
                        if (Zest::string_starts_with(passSampler.sampler, "!"))
                        {
                            passSampler.sampler = Zest::string_left_trim(passSampler.sampler, "!");
                            passSampler.sampleAlternate = true;
                        }
                        spPass->samplers.push_back(passSampler);

                        auto itrFound = spScene->surfaces.find(passSampler.sampler);
                        if (itrFound == spScene->surfaces.end())
                        {
                            AddMessage(*spScene, fmt::format("Sampler not found in pass: {}", passSampler.sampler), MessageSeverity::Error, pSamplerNode->state.row, pSamplerNode->state.col);
                        }
                    }
                    spPass->scriptSamplersLine = int(pSamplerNode->state.row);
                }

                if (hasChild(pPassNode, T_CLEAR))
                {
                    auto pTargetNode = getChild(pPassNode, T_CLEAR);
                    getVector(pTargetNode, spPass->clearColor, 3, 4);
                    spPass->hasClear = true;
                }

                // Complete the pass
                if (spPass->models.empty())
                {
                    AddMessage(*spScene, fmt::format("No geometries in pass: {}", spPass->name), MessageSeverity::Error, pPassNode->state.row);
                }
                else
                {
                    spScene->passes.push_back(spPass);
                    spScene->passNameToIndex[spPass->name] = spScene->passes.size() - 1;
                }

                // If we didn't find targets, add them
                if (spPass->targets.empty())
                {
                    spPass->targets.push_back("default_color");
                    spPass->targets.push_back("default_depth");
                }

                spScene->passOrder.push_back(spPass.get());
            }

            mpc_ast_delete((mpc_ast_t*)r.output);

            // An empty scene, not valid
            if (spScene->passes.empty())
            {
                // We might have found a bad pass and reported it, don't double report.
                if (spScene->errors.empty())
                {
                    AddMessage(*spScene, "No passes found in scene", MessageSeverity::Error);
                }
                // No error here, found earlier
                spScene->valid = false;
            }
        }
        else
        {
            AddMessage(*spScene, sanitize_mpc_error(r.error), MessageSeverity::Error, r.error->state.row, r.error->state.col);
            mpc_err_delete(r.error);
        }
    }
    catch (std::domain_error& ex)
    {
        // Domain error thrown by us
        LOG(DBG, fmt::format("Exception processing scenegraph: {}", ex.what()));
    }
    catch (std::exception& ex)
    {
        AddMessage(*spScene, fmt::format("Exception processing scenegraph: {}", ex.what()), MessageSeverity::Error);
    }

    validate_samplers(*spScene);

    return spScene;
}

void scene_report_error(Scene& scene, MessageSeverity severity, const std::string& txt, const fs::path& path, int32_t line, const std::pair<int32_t, int32_t>& range)
{
    Message msg;
    msg.text = txt;
    if (!path.empty())
    {
        msg.path = path;
    }
    else
    {
        msg.path = scene.sceneGraphPath;
    }

    if (range.first == -1)
    {
        msg.line = line;
    }
    msg.severity = severity;
    msg.range = range;

    // Any error invalidates the scene
    scene.errors.push_back(msg);

    if (msg.severity >= MessageSeverity::Error)
    {
        scene.valid = false;
    }
};

// Finds assets declared in the scene file, first looking in absolute path, then local project path,
// then global assets
fs::path scene_find_asset(Scene& scene, const fs::path& path, AssetType type)
{
    if (path.is_absolute())
    {
        if (fs::exists(path))
        {
            return path;
        }
        return fs::path();
    }

    auto trialPaths = std::vector<fs::path>{
        scene.root / path,
    };

    std::map<AssetType, std::string> subTypePaths = {
        { AssetType::Texture, "textures" },
        { AssetType::Model, "models" },
    };

    auto itr = subTypePaths.find(type);
    if (itr != subTypePaths.end())
    {
        trialPaths.push_back(scene.root / itr->second / path);
    }

    trialPaths.push_back(Zest::runtree_path() / path);

    if (itr != subTypePaths.end())
    {
        trialPaths.push_back(Zest::runtree_path() / itr->second / path);
    }

    for (auto& test : trialPaths)
    {
        if (fs::exists(test))
        {
            auto ret = fs::canonical(fs::absolute(test));
            LOG(DBG, "Found asset at: " << ret.string());
            return ret;
        }
    }
    return fs::path();
}

void scene_copy_state(Scene& destScene, Scene& sourceScene)
{
    // Copy over the old info, if appropriate - this is temporary fix for cleaner solution later.
    for (auto& pPass : destScene.passes)
    {
        auto itrSourcePass = sourceScene.passNameToIndex.find(pPass->name);
        if (itrSourcePass != sourceScene.passNameToIndex.end())
        {
            auto pSourcePass = sourceScene.passes[itrSourcePass->second];
            for (auto& sourceCamName : pSourcePass->cameras)
            {
                for (auto& destCamName : pPass->cameras)
                {
                    if (sourceCamName == destCamName)
                    {
                        auto pDestCam = scene_get_camera(destScene, destCamName);
                        auto pSourceCam = scene_get_camera(sourceScene, sourceCamName);
                        if (pDestCam && pSourceCam)
                        {
                            // Not for now, need to figure out changes
                            //*pDestCam = *pSourceCam;
                        }
                    }
                }
            }
        }
    }
}

bool scene_is_raytracer(const fs::path& f)
{

    if (f.extension() == ".rgen" || f.extension() == ".rmiss" || f.extension() == ".rchit")
    {
        return true;
    }
    return false;
}

bool scene_is_shader(const fs::path& f)
{
    if (scene_is_raytracer(f))
    {
        return true;
    }
    
    if (f.extension() == ".vert" || f.extension() == ".frag" || f.extension() == ".geom")
    {
        return true;
    }
    return false;
}

bool scene_is_header(const fs::path& f)
{
    if (f.extension() == ".h" || f.extension() == ".inc")
    {
        return true;
    }
    return false;
}

bool scene_is_scenegraph(const fs::path& f)
{
    if (f.extension() == ".scenegraph")
    {
        return true;
    }
    return false;
}

bool scene_is_edit_file(const fs::path& f)
{
    if (scene_is_shader(f) || scene_is_header(f) || scene_is_scenegraph(f))
    {
        return true;
    }
    return false;
}