// Loads model; no device specific stuff, just reads it
#include <vklive/file/file.h>
#include <vklive/model.h>
#include <vklive/string/string_utils.h>

#include <assimp/Importer.hpp>
#include <assimp/cimport.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <vklive/logger/logger.h>

const int DefaultModelFlags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;

std::set<std::string> model_file_extensions()
{
    Assimp::Importer importer;
    std::string ext;
    importer.GetExtensionList(ext);

    auto v = string_split(ext, ";* ");
    return std::set<std::string>(v.begin(), v.end());
}

void model_load(Model& model, const std::string& filename, const VertexLayout& layout, const ModelCreateInfo& createInfo, const int flags)
{
    model.layout = layout;
    model.scale = createInfo.scale;
    model.uvscale = createInfo.uvscale;
    model.center = createInfo.center;

    Assimp::Importer importer;
    const aiScene* pScene;

    // Load file
    // auto data = file_read(filename);
    // pScene = importer.ReadFileFromMemory(data.data(), data.size(), flags, filename.c_str());
    pScene = importer.ReadFile(filename.c_str(), flags);

    if (!pScene)
    {
        model.errors = importer.GetErrorString();
        return;
    }

    // Embedded textures
    model.embeddedTextures.clear();
    for (unsigned int i = 0; i < pScene->mNumTextures; i++)
    {
        auto pTex = pScene->mTextures[i];
        ModelTexture tex;
        tex.pathName = pTex->mFilename.C_Str();
        if (tex.pathName.empty())
        {
            tex.pathName = fmt::format("*{}", i);
        }
        tex.size = glm::uvec2(pTex->mWidth, pTex->mHeight);
        if (tex.size.y != 0)
        {
            tex.data.resize(size_t(tex.size.x * tex.size.y * 4));
        }
        else
        {
            tex.data.resize(tex.size.x);
        }
        memcpy(tex.data.data(), pTex->pcData, tex.data.size());

        model.embeddedTextures[tex.pathName] = tex;
    }

    model.materials.clear();
    for (unsigned int i = 0; i < pScene->mNumMaterials; i++)
    {
        auto pMaterial = pScene->mMaterials[i];
        std::string name = pMaterial->GetName().C_Str();
        if (name.empty())
        {
            continue;
        }
        std::vector<std::pair<aiTextureType, ModelTextureType>> mapTypes{
            { aiTextureType_AMBIENT, ModelTextureType::Ambient },

            { aiTextureType_DIFFUSE, ModelTextureType::Diffuse },
            { aiTextureType_BASE_COLOR, ModelTextureType::BaseColor },

            { aiTextureType_LIGHTMAP, ModelTextureType::LightMap },

            { aiTextureType_NORMAL_CAMERA, ModelTextureType::NormalCamera },
            
            { aiTextureType_NORMALS, ModelTextureType::Normal },
            { aiTextureType_HEIGHT, ModelTextureType::Normal },
            { aiTextureType_DISPLACEMENT, ModelTextureType::Normal },

            { aiTextureType_EMISSION_COLOR, ModelTextureType::EmissionColor },
            { aiTextureType_EMISSIVE, ModelTextureType::EmissionColor },

            { aiTextureType_METALNESS, ModelTextureType::Metalness },
            { aiTextureType_SHININESS, ModelTextureType::Metalness },
            { aiTextureType_SPECULAR, ModelTextureType::Metalness },
            { aiTextureType_REFLECTION, ModelTextureType::Metalness },
            { aiTextureType_OPACITY, ModelTextureType::Metalness },

            { aiTextureType_DIFFUSE_ROUGHNESS, ModelTextureType::DiffuseRoughness },
            { aiTextureType_AMBIENT_OCCLUSION, ModelTextureType::AmbientOcclusion },
        };

        ModelMaterial mat;
        for (auto& mapType : mapTypes)
        {
            for (uint32_t texNum = 0; texNum < pMaterial->GetTextureCount(mapType.first); texNum++)
            {
                aiString str;
                pMaterial->GetTexture(mapType.first, texNum, &str);
                LOG(DBG, "MapType: " << (int)mapType.first << ", Index: " << texNum << ", Path: " << str.C_Str());

                auto itrTex = model.embeddedTextures.find(str.C_Str());
                if (itrTex != model.embeddedTextures.end())
                {
                    mat.mapTextures[std::make_pair(mapType.second, texNum)] = &itrTex->second;
                }
            }
        }
        mat.name = pMaterial->GetName().C_Str();
        model.materials.push_back(mat);
    }

    model.parts.clear();
    model.parts.resize(pScene->mNumMeshes);
    for (unsigned int i = 0; i < pScene->mNumMeshes; i++)
    {
        const aiMesh* paiMesh = pScene->mMeshes[i];
        model.parts[i] = {};
        model.parts[i].name = paiMesh->mName.C_Str();
        model.parts[i].vertexBase = model.vertexCount;
        model.parts[i].vertexCount = paiMesh->mNumVertices;
        model.vertexCount += paiMesh->mNumVertices;
    }

    model.vertexData.clear();
    model.indexData.clear();

    model.vertexCount = 0;
    model.indexCount = 0;

    // Load meshes
    for (unsigned int meshIndex = 0; meshIndex < pScene->mNumMeshes; meshIndex++)
    {
        auto& part = model.parts[meshIndex];
        const aiMesh* paiMesh = pScene->mMeshes[meshIndex];
        const auto& numVertices = pScene->mMeshes[meshIndex]->mNumVertices;
        for (unsigned int vertexIndex = 0; vertexIndex < numVertices; vertexIndex++)
        {
            model_append_vertex(model, model.vertexData, pScene, meshIndex, vertexIndex);
        }

        model.dim.size = model.dim.max - model.dim.min;

        model.vertexCount += numVertices;
        part.indexBase = static_cast<uint32_t>(model.indexData.size());
        for (unsigned int j = 0; j < paiMesh->mNumFaces; j++)
        {
            const aiFace& Face = paiMesh->mFaces[j];
            if (Face.mNumIndices != 3)
                continue;
            model.indexData.push_back(part.indexBase + Face.mIndices[0]);
            model.indexData.push_back(part.indexBase + Face.mIndices[1]);
            model.indexData.push_back(part.indexBase + Face.mIndices[2]);
            part.indexCount += 3;
        }
        model.indexCount += part.indexCount;
    }
}

void model_load(Model& model, const std::string& filename, const VertexLayout& layout, float scale, const int flags)
{
    model_load(model, filename, layout, ModelCreateInfo{ glm::vec3(0.0f), glm::vec3(scale), glm::vec2(1.0f) }, flags);
}

void model_append_vertex(Model& model, std::vector<uint8_t>& outputBuffer, const aiScene* pScene, uint32_t meshIndex, uint32_t vertexIndex)
{
    static const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);
    const aiMesh* paiMesh = pScene->mMeshes[meshIndex];
    const auto& j = vertexIndex;
    aiColor3D pColor(0.f, 0.f, 0.f);
    pScene->mMaterials[paiMesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, pColor);
    const aiVector3D* pPos = &(paiMesh->mVertices[j]);
    const aiVector3D* pNormal = &(paiMesh->mNormals[j]);
    const aiVector3D* pTexCoord = (paiMesh->HasTextureCoords(0)) ? &(paiMesh->mTextureCoords[0][j]) : &Zero3D;
    const aiVector3D* pTangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mTangents[j]) : &Zero3D;
    const aiVector3D* pBiTangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mBitangents[j]) : &Zero3D;
    std::vector<float> vertexBuffer;
    glm::vec3 scaledPos{ pPos->x, -pPos->y, pPos->z };
    scaledPos *= model.scale;
    scaledPos += model.center;

    // preallocate float buffer with approximate size
    vertexBuffer.reserve(model.layout.components.size() * 4);
    for (auto& component : model.layout.components)
    {
        switch (component)
        {
        case VERTEX_COMPONENT_POSITION:
            vertexBuffer.push_back(scaledPos.x);
            vertexBuffer.push_back(scaledPos.y);
            vertexBuffer.push_back(scaledPos.z);
            break;
        case VERTEX_COMPONENT_NORMAL:
            vertexBuffer.push_back(pNormal->x);
            vertexBuffer.push_back(-pNormal->y);
            vertexBuffer.push_back(pNormal->z);
            break;
        case VERTEX_COMPONENT_UV:
            vertexBuffer.push_back(pTexCoord->x * model.uvscale.s);
            vertexBuffer.push_back(pTexCoord->y * model.uvscale.t);
            break;
        case VERTEX_COMPONENT_COLOR:
            vertexBuffer.push_back(pColor.r);
            vertexBuffer.push_back(pColor.g);
            vertexBuffer.push_back(pColor.b);
            break;
        case VERTEX_COMPONENT_TANGENT:
            vertexBuffer.push_back(pTangent->x);
            vertexBuffer.push_back(pTangent->y);
            vertexBuffer.push_back(pTangent->z);
            break;
        case VERTEX_COMPONENT_BITANGENT:
            vertexBuffer.push_back(pBiTangent->x);
            vertexBuffer.push_back(pBiTangent->y);
            vertexBuffer.push_back(pBiTangent->z);
            break;
        // Dummy components for padding
        case VERTEX_COMPONENT_DUMMY_INT:
        case VERTEX_COMPONENT_DUMMY_FLOAT:
            vertexBuffer.push_back(0.0f);
            break;
        case VERTEX_COMPONENT_DUMMY_INT4:
        case VERTEX_COMPONENT_DUMMY_UINT4:
        case VERTEX_COMPONENT_DUMMY_VEC4:
            vertexBuffer.push_back(0.0f);
            vertexBuffer.push_back(0.0f);
            vertexBuffer.push_back(0.0f);
            vertexBuffer.push_back(0.0f);
            break;
        };
    }
    vector_bytes_append(outputBuffer, vertexBuffer);

    model.dim.max = glm::max(scaledPos, model.dim.max);
    model.dim.min = glm::min(scaledPos, model.dim.min);
}

uint32_t component_index(const VertexLayout& layout, Component component)
{
    for (size_t i = 0; i < layout.components.size(); ++i)
    {
        if (layout.components[i] == component)
        {
            return (uint32_t)i;
        }
    }
    return static_cast<uint32_t>(-1);
}

uint32_t component_size(Component component)
{
    switch (component)
    {
    case VERTEX_COMPONENT_UV:
        return 2 * sizeof(float);
    case VERTEX_COMPONENT_DUMMY_FLOAT:
        return sizeof(float);
    case VERTEX_COMPONENT_DUMMY_INT:
        return sizeof(int);
    case VERTEX_COMPONENT_DUMMY_VEC4:
        return 4 * sizeof(float);
    case VERTEX_COMPONENT_DUMMY_INT4:
        return 4 * sizeof(int32_t);
    case VERTEX_COMPONENT_DUMMY_UINT4:
        return 4 * sizeof(uint32_t);
    default:
        // All components except the ones listed above are made up of 3 floats
        return 3 * sizeof(float);
    }
}

uint32_t layout_size(const VertexLayout& layout)
{
    uint32_t res = 0;
    for (auto& component : layout.components)
    {
        res += component_size(component);
    }
    return res;
}

uint32_t layout_offset(const VertexLayout& layout, uint32_t index)
{
    uint32_t res = 0;
    assert(index < layout.components.size());
    for (uint32_t i = 0; i < index; ++i)
    {
        res += component_size(layout.components[i]);
    }
    return res;
}
