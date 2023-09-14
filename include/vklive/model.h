#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <set>
#include <string>

struct aiScene;
namespace Assimp
{
class Importer;
};

// Layout
enum Component
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
    std::vector<Component> components;
    bool operator==(const VertexLayout& rhs) const
    {
        return rhs.components == components;
    }
};

extern VertexLayout g_vertexLayout;

// Create info
struct ModelCreateInfo
{
    std::string filename;
    VertexLayout vertexLayout = g_vertexLayout;
    glm::vec3 center{ 0 };
    glm::vec3 scale{ 1 };
    glm::vec2 uvscale{ 1 };
    bool buildAS = false;
    
    size_t hash() const
    {
        using namespace std;
        
        size_t result = std::hash<std::string>()(filename);
        result ^= std::hash<glm::vec3>()(center);
        result ^= std::hash<glm::vec3>()(scale);
        result ^= std::hash<glm::vec2>()(uvscale);
        result ^= buildAS ? 1 : 0;
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

struct ModelMaterial
{
    std::string name;
    glm::vec4 diffuse;
    glm::vec4 ambient;
    glm::vec4 specular;
    glm::vec4 emissive;
    glm::vec4 reflective;
    std::map<std::pair<ModelTextureType, int>, ModelTexture*> mapTextures;
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

uint32_t component_index(const VertexLayout& layout, Component component);
uint32_t component_size(Component component);
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
