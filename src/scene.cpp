#include <algorithm>
#include <fmt/format.h>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "toml++/toml.h"

#include <vklive/file/file.h>
#include <vklive/file/runtree.h>
#include <vklive/logger/logger.h>
#include <vklive/scene.h>
#include <vklive/string/string_utils.h>

#include "config_app.h"

extern "C" {
#include "mpc/mpc.h"
}

#define T_PATH_NAME "path_name"
#define T_PATH "path"
#define T_COMMENT "comment"
#define T_IDENT "ident"
#define T_FLOAT "float"
#define T_ASSIGN "assign"
#define T_VECTOR "vector"
#define T_GEOMETRY "geometry"
#define T_SCALE "scale"
#define T_CLEAR "clear"
#define T_DISABLE "disable"
#define T_PASS "pass"
#define T_VS "vs"
#define T_FS "fs"
#define T_GS "gs"
#define T_SCENEGRAPH "scenegraph"

#include <concurrentqueue/concurrentqueue.h>

struct Parser
{
    mpc_parser_t* pSceneGraph = nullptr;
    std::vector<mpc_parser_t*> parsers;
    mpc_err_t* pError = nullptr;
};
Parser parser;

enum class ShaderType
{
    Vertex,
    Geometry,
    Fragment
};

const auto ShaderTypes = std::map<std::string, ShaderType>{
    { T_VS, ShaderType::Vertex },
    { T_GS, ShaderType::Geometry },
    { T_FS, ShaderType::Fragment }
};

std::string sanitize_mpc_error(mpc_err_t* pErr)
{
    auto errString = std::string(mpc_err_string(pErr));
    auto startPos = errString.find("error:");
    if (startPos != std::string::npos)
    {
        errString = errString.substr(startPos);
    }
    return string_trim(errString);
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

    ADD_PARSER(path_name, T_PATH_NAME);
    ADD_PARSER(path_id, T_PATH);
    ADD_PARSER(comment, T_COMMENT);
    ADD_PARSER(ident, T_IDENT);
    ADD_PARSER(flt, T_FLOAT);
    ADD_PARSER(assign, T_ASSIGN);
    ADD_PARSER(geometry, T_GEOMETRY);
    ADD_PARSER(scale, T_SCALE);
    ADD_PARSER(vector, T_VECTOR);
    ADD_PARSER(clear, T_CLEAR);
    ADD_PARSER(disable, T_DISABLE);
    ADD_PARSER(pass, T_PASS);
    ADD_PARSER(vs, T_VS);
    ADD_PARSER(gs, T_GS);
    ADD_PARSER(fs, T_FS);

    // Special case; we hold onto it.
    parser.pSceneGraph = mpc_new(T_SCENEGRAPH);
    parser.parsers.push_back(parser.pSceneGraph);

    parser.pError = mpca_lang(MPCA_LANG_DEFAULT, R"(
path_name        : /[a-zA-Z_][a-zA-Z0-9_\/.]*/ ;
path             : "path" ":" <path_name> ;
comment          : /\/\/[^\n\r]*/ ;
ident            : /[a-zA-Z_][a-zA-Z0-9_]*/ ;
float            : /[+-]?\d+(\.\d+)?([eE][+-]?[0-9]+)?/ ;
vector           : '(' <float> ','? <float> ','? <float> (','? <float>)? ')' ;
scale            : "scale" ':' <vector> ;
clear            : "clear" ":" <vector> ;
vs               : "vs" ':' <path_name> ;
gs               : "gs" ':' <path_name> ;
fs               : "fs" ':' <path_name> ;
geometry         : "geometry" ":" <ident> '{' (<path> | <scale> | <vs> | <fs> | <gs> | <comment>)* '}';
disable          : '!' ;
pass             : <disable>? "pass" ':' <ident> '{' (<geometry> | <comment> | <clear>)* '}'; 
scenegraph       : /^/ (<comment> | <pass>)* /$/ ;
    )", path_name, path_id, comment, ident, flt, vector, assign, geometry, scale, clear, disable, pass, vs, gs, fs, parser.pSceneGraph, nullptr);
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
            catch(std::exception& ex)
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
            file_write(sceneGraphPath, "# Scenegraph");
        }
        catch (std::exception& ex)
        {
        }
    }
    return sceneGraphPath;
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

std::shared_ptr<Scene> scene_build(const fs::path& root)
{
    std::shared_ptr<Scene> spScene = std::make_shared<Scene>(root);

    auto files = file_gather_files(root);

    spScene->sceneGraphPath = scene_get_scenegraph(root, files);
    spScene->headers = scene_get_headers(files);
    spScene->valid = true;

    auto addError = [&](auto message, uint32_t lineIndex = 0, int32_t column = -1) {
        Message msg;
        msg.severity = MessageSeverity::Error;
        msg.path = spScene->sceneGraphPath;
        msg.line = lineIndex;
        if (column != -1)
        {
            msg.range = std::make_pair(column, column + 1);
        }
        msg.text = message;
        spScene->errors.push_back(msg);
        spScene->valid = false;
        LOG(DBG, message);
    };

    scene_init_parser();

    // Add the error to this scene's file
    if (parser.pError != NULL)
    {
        addError(sanitize_mpc_error(parser.pError), parser.pError->state.row, parser.pError->state.col);
        return spScene;
    }

    try
    {
        int passStartLine = 0;
        mpc_result_t r;
        if (mpc_parse_contents(spScene->sceneGraphPath.string().c_str(), parser.pSceneGraph, &r))
        {
            auto ast_current = (mpc_ast_t*)r.output;
            mpc_ast_print((mpc_ast_t*)r.output);

            auto childrenOf = [&](mpc_ast_t* entry, const std::string& val) {
                std::vector<mpc_ast_t*> children;
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
                throw std::domain_error(fmt::format("tag not found {}", tags.str()).c_str());
            };

            LOG(DBG, "Tag: " << ast_current->tag << " Contents: " << ast_current->contents);

            auto passes = childrenOf(ast_current, T_PASS);

            for (auto& pPassNode : passes)
            {
                // Find the next pass
                auto pPassNameNode = getChild(pPassNode, T_IDENT);
                if (hasChild(pPassNode, T_DISABLE))
                {
                    continue;
                }
                auto spPass = std::make_shared<Pass>(pPassNameNode->contents);

                auto geometries = childrenOf(pPassNode, T_GEOMETRY);
                for (auto& pGeometryNode : geometries)
                {
                    auto pGeomNameNode = getChild(pGeometryNode, T_IDENT);
                    auto pPathNameNode = getChild(getChild(pGeometryNode, T_PATH), T_PATH_NAME);

                    // Path
                    auto geomPath = root / pPathNameNode->contents;

                    std::shared_ptr<Geometry> spGeom;

                    if (geomPath.filename() == "screen_rect")
                    {
                        spGeom = std::make_shared<Geometry>(GeometryType::Rect);
                    }
                    else
                    {
                        if (!fs::exists(geomPath))
                        {
                            addError(std::string("Geometry missing: " + geomPath.filename().string()), pGeometryNode->state.row);
                            continue;
                        }
                        spGeom = std::make_shared<Geometry>(geomPath);
                    }

                    // Shaders
                    for (auto& [key, value] : ShaderTypes)
                    {
                        auto shaderEntries = childrenOf(pGeometryNode, key);
                        for (auto& pShaderEntry : shaderEntries)
                        {
                            auto pPathNode = getChild(pShaderEntry, T_PATH);
                            auto shaderPath = root / pPathNode->contents;
                            if (!fs::exists(shaderPath))
                            {
                                addError(std::string("Shader missing: " + shaderPath.filename().string()), pPathNode->state.pos);
                                continue;
                            }
                            auto spShaderFrag = std::make_shared<Shader>(shaderPath);
                            spScene->shaders[spShaderFrag->path] = spShaderFrag;
                            spPass->shaders.push_back(shaderPath);
                        }
                    }

                    if (spPass->shaders.empty())
                    {
                        addError(fmt::format("No shaders in geometry: {}", pGeomNameNode->contents), pGeomNameNode->state.row);
                        continue;
                    }

                    // Scale
                    auto scaleEntries = childrenOf(pGeometryNode, T_SCALE);
                    for (auto& pScaleNode : scaleEntries)
                    {
                        auto pVecNode = getChild(pScaleNode, T_VECTOR);
                        auto vals = childrenOf(pVecNode, T_FLOAT);
                        for (int i = 0; i < std::min(3, int(vals.size())); i++)
                        {
                            // Temporarily do it at load time
                            spGeom->loadScale[i] = std::stof(vals[i]->contents);
                        }

                    }

                    spScene->geometries[geomPath] = spGeom;
                    spPass->geometries.push_back(geomPath);
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


                // Complete the pass
                if (spPass->geometries.empty())
                {
                    addError(fmt::format("No geometries in pass: {}", spPass->name), pPassNode->state.row);
                }
                else
                {
                    spScene->passes[spPass->name] = spPass;
                }
            }

            mpc_ast_delete((mpc_ast_t*)r.output);

            // An empty scene, not valid
            if (spScene->passes.empty())
            {
                // We might have found a bad pass and reported it, don't double report.
                if (spScene->errors.empty())
                {
                    addError("No passes found in scene");
                }
                // No error here, found earlier
                spScene->valid = false;
            }
        }
        else
        {
            addError(sanitize_mpc_error(r.error), r.error->state.row, r.error->state.col);
            mpc_err_delete(r.error);
        }
    }
    catch (std::exception& ex)
    {
        addError(fmt::format("Exception processing scenegraph: {}", ex.what()));
    }

    return spScene;
}

