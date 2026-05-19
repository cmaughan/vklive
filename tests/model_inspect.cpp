#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <vklive/model.h>

namespace
{
bool require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

std::vector<glm::vec2> collect_uvs(const Model& model)
{
    std::vector<glm::vec2> uvs;
    const auto uvIndex = component_index(model.createInfo.vertexLayout, VERTEX_COMPONENT_UV);
    const auto uvOffset = layout_offset(model.createInfo.vertexLayout, uvIndex);
    const auto stride = layout_size(model.createInfo.vertexLayout);
    uvs.reserve(model.vertexCount);

    for (uint32_t i = 0; i < model.vertexCount; ++i)
    {
        glm::vec2 uv;
        std::memcpy(&uv, model.vertexData.data() + (i * stride) + uvOffset, sizeof(uv));
        uvs.push_back(uv);
    }

    return uvs;
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: vklive_model_tests <scene.gltf>\n";
        return EXIT_FAILURE;
    }

    Model model;
    ModelCreateInfo info;
    info.filename = argv[1];
    model_load(model, info);

    Model upperLeftModel;
    ModelCreateInfo upperLeftInfo = info;
    upperLeftInfo.uvOrigin = ModelUvOrigin::UpperLeft;
    model_load(upperLeftModel, upperLeftInfo);

    bool ok = true;
    ok &= require(model.errors.empty(), "model loader reported: " + model.errors);
    ok &= require(model.parts.size() == 3, "robot should load 3 mesh parts, got " + std::to_string(model.parts.size()));
    ok &= require(model.materials.size() >= 3, "robot should load at least 3 materials, got " + std::to_string(model.materials.size()));
    ok &= require(model.vertexCount > 0, "robot should have vertices");
    ok &= require(model.indexCount > 0, "robot should have indices");

    for (const auto& part : model.parts)
    {
        ok &= require(part.vertexCount > 0, "part has no vertices: " + part.name);
        ok &= require(part.indexCount > 0, "part has no indices: " + part.name);
        ok &= require(part.materialIndex < model.materials.size(), "part material index out of range: " + part.name);
        for (uint32_t i = part.indexBase; i < part.indexBase + part.indexCount; ++i)
        {
            ok &= require(model.indexData[i] < model.vertexCount, "index references vertex outside loaded buffer");
        }
    }

    ok &= require(model.materials[model.parts[0].materialIndex].name.find("RobotChest") != std::string::npos,
        "Object_0 should use RobotChest material, got " + model.materials[model.parts[0].materialIndex].name);
    ok &= require(model.materials[model.parts[1].materialIndex].name.find("RobotHead") != std::string::npos,
        "Object_1 should use RobotHead material, got " + model.materials[model.parts[1].materialIndex].name);
    ok &= require(model.materials[model.parts[2].materialIndex].name.find("RobotExtremities") != std::string::npos,
        "Object_2 should use RobotExtremities material, got " + model.materials[model.parts[2].materialIndex].name);

    const ModelMaterial* chestMaterial = nullptr;
    for (const auto& material : model.materials)
    {
        if (material.textures.baseColor.pathName.find("RobotChest_baseColor") != std::string::npos)
        {
            chestMaterial = &material;
            break;
        }
    }

    ok &= require(chestMaterial != nullptr, "missing chest material");
    if (chestMaterial)
    {
        ok &= require(chestMaterial->textures.baseColor.pathName.find("RobotChest_baseColor") != std::string::npos, "missing chest base color texture");
        ok &= require(chestMaterial->textures.normal.pathName.find("RobotChest_normal") != std::string::npos, "missing chest normal texture");
        ok &= require(chestMaterial->textures.metallicRoughness.pathName.find("RobotChest_metallicRoughness") != std::string::npos, "missing chest metallic roughness texture");
        ok &= require(chestMaterial->textures.emissive.pathName.find("RobotChest_emissive") != std::string::npos, "missing chest emissive texture");
    }

    ok &= require(info.uvOrigin == ModelUvOrigin::LowerLeft, "default model UV origin should match lower-left authored UVs");
    ok &= require(component_index(info.vertexLayout, VERTEX_COMPONENT_TANGENT) != static_cast<uint32_t>(-1),
        "default model vertex layout should include tangents for normal mapping");
    ok &= require(component_index(info.vertexLayout, VERTEX_COMPONENT_BITANGENT) != static_cast<uint32_t>(-1),
        "default model vertex layout should include bitangents for normal mapping");

    ok &= require(upperLeftModel.errors.empty(), "upper-left model loader reported: " + upperLeftModel.errors);
    ok &= require(upperLeftModel.vertexCount == model.vertexCount, "UV origin should not change vertex count");

    const auto uvs = collect_uvs(model);
    const auto upperLeftUvs = collect_uvs(upperLeftModel);
    bool allUvsPreserved = uvs.size() == upperLeftUvs.size();
    for (size_t i = 0; i < std::min(uvs.size(), upperLeftUvs.size()); ++i)
    {
        allUvsPreserved = allUvsPreserved &&
            std::abs(uvs[i].x - upperLeftUvs[i].x) < 0.0001f &&
            std::abs(uvs[i].y - upperLeftUvs[i].y) < 0.0001f;
    }
    ok &= require(allUvsPreserved, "uv_origin should flip material textures, not imported UV coordinates");

    ok &= require(chestMaterial == nullptr || chestMaterial->textures.baseColor.flipY,
        "lower-left model UV origin should flip base color texture rows on upload");

    const ModelMaterial* upperLeftChestMaterial = nullptr;
    for (const auto& material : upperLeftModel.materials)
    {
        if (material.textures.baseColor.pathName.find("RobotChest_baseColor") != std::string::npos)
        {
            upperLeftChestMaterial = &material;
            break;
        }
    }
    ok &= require(upperLeftChestMaterial != nullptr, "missing upper-left chest material");
    ok &= require(upperLeftChestMaterial == nullptr || !upperLeftChestMaterial->textures.baseColor.flipY,
        "upper-left model UV origin should upload base color texture rows unchanged");

    Model reusedModel;
    model_load(reusedModel, info);
    model_load(reusedModel, upperLeftInfo);
    const ModelMaterial* reusedChestMaterial = nullptr;
    for (const auto& material : reusedModel.materials)
    {
        if (material.textures.baseColor.pathName.find("RobotChest_baseColor") != std::string::npos)
        {
            reusedChestMaterial = &material;
            break;
        }
    }
    ok &= require(reusedModel.createInfo.uvOrigin == ModelUvOrigin::UpperLeft,
        "model_load should refresh when UV origin changes on an existing Model");
    ok &= require(reusedChestMaterial == nullptr || !reusedChestMaterial->textures.baseColor.flipY,
        "reloaded upper-left model should update material texture flip state");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
