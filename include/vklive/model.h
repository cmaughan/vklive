#pragma once

#include <cfloat>
#include <cstdint>
#include <functional>
#include <map>
#include <utility>

#include <glm/glm.hpp>
#include <vector>
#include <set>
#include <string>

#include <zest/file/file.h>

struct aiScene;
namespace Assimp
{
class Importer;
};

// Layout
enum VertexComponent
{
    VERTEX_COMPONENT_POSITION = 0x0,
    VERTEX_COMPONENT_NORMAL = 0x1,
    VERTEX_COMPONENT_COLOR = 0x2,
    VERTEX_COMPONENT_UV = 0x3,
    VERTEX_COMPONENT_TANGENT = 0x4,
    VERTEX_COMPONENT_BITANGENT = 0x5,
    VERTEX_COMPONENT_DUMMY_FLOAT = 0x6,
    VERTEX_COMPONENT_DUMMY_INT = 0x7,
    VERTEX_COMPONENT_DUMMY_VEC4 = 0x8,
    VERTEX_COMPONENT_DUMMY_INT4 = 0x9,
    VERTEX_COMPONENT_DUMMY_UINT4 = 0xA,
};

struct VertexLayout
{
    std::vector<VertexComponent> components;
    bool operator==(const VertexLayout& rhs) const
    {
        return rhs.components == components;
    }
};

extern VertexLayout g_vertexLayout;

enum class ModelUvOrigin
{
    UpperLeft,
    LowerLeft
};

// Create info
struct ModelCreateInfo
{
    std::string filename;
    VertexLayout vertexLayout = g_vertexLayout;
    glm::vec3 center{ 0 };
    glm::vec3 scale{ 1 };
    glm::vec2 uvscale{ 1 };
    ModelUvOrigin uvOrigin = ModelUvOrigin::LowerLeft;
    bool buildAS = false;
    
    size_t hash() const
    {
        auto hashCombine = [](size_t& seed, size_t value) {
            seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };

        size_t result = 0;
        hashCombine(result, std::hash<std::string>()(filename));
        for (const auto& component : vertexLayout.components)
        {
            hashCombine(result, std::hash<int>()(static_cast<int>(component)));
        }
        hashCombine(result, std::hash<float>()(center.x));
        hashCombine(result, std::hash<float>()(center.y));
        hashCombine(result, std::hash<float>()(center.z));
        hashCombine(result, std::hash<float>()(scale.x));
        hashCombine(result, std::hash<float>()(scale.y));
        hashCombine(result, std::hash<float>()(scale.z));
        hashCombine(result, std::hash<float>()(uvscale.x));
        hashCombine(result, std::hash<float>()(uvscale.y));
        hashCombine(result, std::hash<int>()(static_cast<int>(uvOrigin)));
        hashCombine(result, std::hash<bool>()(buildAS));
        return result;
    }
    
    bool operator==(const ModelCreateInfo& createInfo) const
    {
        return 
            (createInfo.filename == filename) &&
            (createInfo.vertexLayout == vertexLayout) &&
            (createInfo.center == center) && 
            (createInfo.scale == scale) &&
            (createInfo.uvscale == uvscale) &&
            (createInfo.uvOrigin == uvOrigin) &&
            (createInfo.buildAS == buildAS);
    }
};

struct ModelCreateInfoHash
{
    std::size_t operator()(const ModelCreateInfo& g) const
    {
        return g.hash();
    }
};

// We support PBR style of models
enum ModelTextureType
{
    // Others
    Ambient,

    // PBR
    BaseColor,
    Diffuse,
    LightMap,
    NormalCamera,
    Normal,
    EmissionColor,
    Metalness,
    DiffuseRoughness,
    AmbientOcclusion
};

struct ModelTexture
{
    glm::uvec2 size;
    std::string pathName;
    std::vector<uint8_t> data;
};

struct ModelTextureSlot
{
    std::string pathName;
    fs::path resolvedPath;
    ModelTexture* embeddedTexture = nullptr;
    bool srgb = false;
    bool flipY = false;

    bool valid() const
    {
        return embeddedTexture || !resolvedPath.empty();
    }
};

struct ModelMaterialTextures
{
    ModelTextureSlot baseColor;
    ModelTextureSlot normal;
    ModelTextureSlot metallicRoughness;
    ModelTextureSlot emissive;
    ModelTextureSlot occlusion;
};

struct ModelMaterial
{
    std::string name;
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    glm::vec4 emissiveFactor = glm::vec4(0.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float occlusionStrength = 1.0f;
    ModelMaterialTextures textures;
};

// Model
struct Model
{
    struct ModelPart
    {
        std::string name;
        uint32_t vertexBase;
        uint32_t vertexCount;
        uint32_t indexBase;
        uint32_t indexCount;
        uint32_t materialIndex = 0;
    };
    std::vector<ModelPart> parts;

    std::vector<ModelMaterial> materials;

    std::map<std::string, ModelTexture> embeddedTextures;

    struct Dimension
    {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
        glm::vec3 size;
    };

    Dimension dim;
    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;
    
    std::vector<uint8_t> vertexData;
    std::vector<uint32_t> indexData;

    std::string errors;

    ModelCreateInfo createInfo;
    bool loaded = false;
    fs::file_time_type lastWrite;
    int32_t refCount = 0;
};

extern const int DefaultModelFlags;

uint32_t component_index(const VertexLayout& layout, VertexComponent component);
uint32_t component_size(VertexComponent component);
uint32_t layout_size(const VertexLayout& layout);
uint32_t layout_offset(const VertexLayout& layout, uint32_t index);
void model_load(Model& model, const ModelCreateInfo& createInfo, int flags = DefaultModelFlags);
void model_append_vertex(Model& model, std::vector<uint8_t>& outputBuffer, const aiScene* pScene, uint32_t meshIndex, uint32_t vertexIndex);

std::set<std::string> model_file_extensions();

template <typename T>
void vector_bytes_append(std::vector<uint8_t>& outputBuffer, const T& t)
{
    auto offset = outputBuffer.size();
    auto copySize = sizeof(T);
    outputBuffer.resize(offset + copySize);
    memcpy(outputBuffer.data() + offset, &t, copySize);
}

template <typename T>
void vector_bytes_append(std::vector<uint8_t>& outputBuffer, const std::vector<T>& v)
{
    auto offset = outputBuffer.size();
    auto copySize = v.size() * sizeof(T);
    outputBuffer.resize(offset + copySize);
    memcpy(outputBuffer.data() + offset, v.data(), copySize);
}
