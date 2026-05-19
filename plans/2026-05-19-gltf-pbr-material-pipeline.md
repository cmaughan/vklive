# glTF PBR Material Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Load all glTF mesh sections correctly, preserve each mesh part's material assignment, bind standard glTF PBR textures through engine-owned shader bindings, and update the `pbr_robot` sample to use material-type samplers instead of body-part-specific scene samplers.

**Architecture:** Keep scene-declared samplers as user-controlled pass inputs on descriptor set 1, and add an engine-reserved material interface on descriptor set 2. The CPU model loader will resolve glTF materials, texture paths, material factors, and per-mesh material indices. The Vulkan model path will upload material factors, load material textures with fallback resources, bind a per-model material descriptor set, and draw each `ModelPart` separately with a push constant selecting the active material.

**Tech Stack:** C++20, Assimp glTF import, stb_image/Vulkan image upload through existing `VulkanSurface`, Vulkan descriptor sets, push constants, GLSL 450 includes, CMake via `config.bat` and `build.bat`.

---

## Root Cause Summary

The current sample-level fix is structurally unable to be correct:

- `src/model.cpp` only stores embedded material textures. The RTS robot glTF references external files such as `textures/RobotChest_baseColor.jpeg`, so the real glTF material maps are not available to the renderer.
- `ModelPart` does not store `aiMesh::mMaterialIndex`, so the renderer cannot know which material belongs to each mesh section.
- `src/vulkan/vulkan_pass.cpp` draws each model once with `cmd.drawIndexed(pVulkanGeom->indexCount, 1, 0, 0, 0)`, so there is no per-part draw point where a material can be selected.
- `src/model.cpp` currently builds indices with `part.indexBase + Face.mIndices[n]`. `indexBase` is an index-buffer offset, not a vertex-buffer offset. For multi-mesh glTF files this can reference the wrong vertex range; the robot has three meshes and three materials, so this is a credible cause of missing or scrambled sections.
- Pass samplers are mapped by arbitrary scene names. That is good for environment maps and user textures, but glTF PBR needs standard material slots: base color, normal, metallic-roughness, emissive, and occlusion.

---

## Standard Shader Interface

Reserve these bindings:

- `set = 0`: existing `UBO` from `default_parameters.h`.
- `set = 1`: existing scene pass samplers, bound by scene sampler name, for user-controlled inputs such as `studio_sky`.
- `set = 2`: engine-provided model material inputs. Users opt in by including `vklive_pbr_material.glsl`.

Create `run_tree/shaders/include/vklive_pbr_material.glsl` with this public contract:

```glsl
#ifndef VKLIVE_PBR_MATERIAL_GLSL
#define VKLIVE_PBR_MATERIAL_GLSL

#define VKLIVE_MAX_MATERIALS 64

struct VklMaterial
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 metallicRoughnessOcclusion;
    ivec4 textureIndices;
};

layout(push_constant) uniform VklDraw
{
    uint materialIndex;
} vklDraw;

layout(std430, set = 2, binding = 0) readonly buffer VklMaterials
{
    VklMaterial vklMaterials[];
};

layout(set = 2, binding = 1) uniform sampler2D vklBaseColorTextures[VKLIVE_MAX_MATERIALS];
layout(set = 2, binding = 2) uniform sampler2D vklNormalTextures[VKLIVE_MAX_MATERIALS];
layout(set = 2, binding = 3) uniform sampler2D vklMetallicRoughnessTextures[VKLIVE_MAX_MATERIALS];
layout(set = 2, binding = 4) uniform sampler2D vklEmissiveTextures[VKLIVE_MAX_MATERIALS];
layout(set = 2, binding = 5) uniform sampler2D vklOcclusionTextures[VKLIVE_MAX_MATERIALS];

vec4 vklBaseColor(vec2 uv)
{
    uint index = min(vklDraw.materialIndex, uint(VKLIVE_MAX_MATERIALS - 1));
    return texture(vklBaseColorTextures[index], uv) * vklMaterials[index].baseColorFactor;
}

vec3 vklNormalSample(vec2 uv)
{
    uint index = min(vklDraw.materialIndex, uint(VKLIVE_MAX_MATERIALS - 1));
    return texture(vklNormalTextures[index], uv).xyz * 2.0 - 1.0;
}

vec2 vklMetallicRoughness(vec2 uv)
{
    uint index = min(vklDraw.materialIndex, uint(VKLIVE_MAX_MATERIALS - 1));
    vec4 sampleValue = texture(vklMetallicRoughnessTextures[index], uv);
    vec4 factors = vklMaterials[index].metallicRoughnessOcclusion;
    return vec2(sampleValue.b * factors.x, sampleValue.g * factors.y);
}

vec3 vklEmissive(vec2 uv)
{
    uint index = min(vklDraw.materialIndex, uint(VKLIVE_MAX_MATERIALS - 1));
    return texture(vklEmissiveTextures[index], uv).rgb * vklMaterials[index].emissiveFactor.rgb;
}

float vklOcclusion(vec2 uv)
{
    uint index = min(vklDraw.materialIndex, uint(VKLIVE_MAX_MATERIALS - 1));
    return texture(vklOcclusionTextures[index], uv).r * vklMaterials[index].metallicRoughnessOcclusion.z;
}

#endif
```

Naming rule for users: use the helper functions above, not body-part sampler names. Scene samplers remain useful for environment lighting:

```glsl
layout(set = 1, binding = 0) uniform sampler2D studio_sky;
#include "vklive_pbr_material.glsl"
```

---

## File Structure

- Modify `include/vklive/model.h`: add texture slot metadata, PBR material factors, and `ModelPart::materialIndex`.
- Modify `src/model.cpp`: resolve external glTF textures, store material factors, fix multi-mesh index generation.
- Modify `include/vklive/vulkan/vulkan_model.h`: store material GPU buffer, material texture surfaces, material descriptors, and fallback texture references.
- Modify `src/vulkan/vulkan_model.cpp`: load material textures, create fallback textures, upload material buffers, and destroy resources safely.
- Modify `src/vulkan/vulkan_pass.cpp`: support descriptor arrays, reserve set 2 for model material descriptors, add push constant range, bind material descriptor set per model, and draw parts.
- Modify `src/vulkan/vulkan_pipeline.cpp`: accept a push constant range in the pipeline layout path or create a standard graphics push constant range.
- Create `run_tree/shaders/include/vklive_pbr_material.glsl`: documented standard shader include.
- Modify `run_tree/projects/pbr_robot/default.scenegraph`: remove body-part texture `surface`s and keep only `studio_sky` as a scene sampler.
- Modify `run_tree/projects/pbr_robot/pbr.frag`: use `vklive_pbr_material.glsl` helpers.
- Modify `tests/test_pbr_template.py`: assert standard material include and no body-part samplers.
- Create `tests/model_inspect.cpp`: non-GUI model loader checks.
- Modify `CMakeLists.txt`: add `vklive_model_tests` and register it with CTest.

---

### Task 1: Add Model Loader Regression Test

**Files:**
- Create: `tests/model_inspect.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add failing C++ model inspection test**

Create `tests/model_inspect.cpp`:

```cpp
#include <cstdlib>
#include <iostream>
#include <string>

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

    bool ok = true;
    ok &= require(model.errors.empty(), "model loader reported: " + model.errors);
    ok &= require(model.parts.size() == 3, "robot should load 3 mesh parts");
    ok &= require(model.materials.size() == 3, "robot should load 3 materials");
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

    ok &= require(model.materials[0].textures.baseColor.pathName.find("RobotChest_baseColor") != std::string::npos, "missing chest base color texture");
    ok &= require(model.materials[0].textures.normal.pathName.find("RobotChest_normal") != std::string::npos, "missing chest normal texture");
    ok &= require(model.materials[0].textures.metallicRoughness.pathName.find("RobotChest_metallicRoughness") != std::string::npos, "missing chest metallic roughness texture");
    ok &= require(model.materials[0].textures.emissive.pathName.find("RobotChest_emissive") != std::string::npos, "missing chest emissive texture");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

- [ ] **Step 2: Add test target**

In `CMakeLists.txt`, after `add_library(vklive ${LIB_SOURCES})` and its includes/link setup, add:

```cmake
enable_testing()

add_executable(vklive_model_tests tests/model_inspect.cpp)
target_link_libraries(vklive_model_tests PRIVATE vklive)
target_include_directories(vklive_model_tests PRIVATE ${CMAKE_BINARY_DIR})

add_test(
    NAME vklive_model_tests_robot
    COMMAND vklive_model_tests ${CMAKE_CURRENT_LIST_DIR}/run_tree/projects/pbr_robot/models/robot/scene.gltf
)
```

- [ ] **Step 3: Run and confirm red**

Run:

```powershell
.\config.bat
.\build.bat
ctest --test-dir build -C Debug --output-on-failure -R vklive_model_tests_robot
```

Expected: compile fails because `ModelMaterial::textures` and `ModelPart::materialIndex` do not exist. If compilation is temporarily adjusted before implementation, the runtime assertions fail because external textures are not resolved and the current index generation is wrong for multi-mesh models.

---

### Task 2: Extend CPU Model Material Data

**Files:**
- Modify: `include/vklive/model.h`
- Modify: `src/model.cpp`

- [ ] **Step 1: Add texture slot and PBR material structures**

Replace the pointer-based material texture map with named slots that can refer to embedded or external texture sources:

```cpp
struct ModelTextureSlot
{
    std::string pathName;
    fs::path resolvedPath;
    ModelTexture* embeddedTexture = nullptr;
    bool srgb = false;
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
```

Add to `Model::ModelPart`:

```cpp
uint32_t materialIndex = 0;
```

- [ ] **Step 2: Resolve texture slots**

In `src/model.cpp`, add a helper near `model_load`:

```cpp
ModelTextureSlot model_resolve_texture_slot(Model& model, const fs::path& modelPath, aiMaterial* pMaterial, aiTextureType textureType, bool srgb)
{
    ModelTextureSlot slot;
    if (pMaterial->GetTextureCount(textureType) == 0)
    {
        return slot;
    }

    aiString str;
    if (pMaterial->GetTexture(textureType, 0, &str) != aiReturn_SUCCESS)
    {
        return slot;
    }

    slot.pathName = str.C_Str();
    slot.srgb = srgb;

    auto embedded = model.embeddedTextures.find(slot.pathName);
    if (embedded != model.embeddedTextures.end())
    {
        slot.embeddedTexture = &embedded->second;
        return slot;
    }

    fs::path texturePath = fs::path(slot.pathName);
    if (texturePath.is_relative())
    {
        texturePath = modelPath.parent_path() / texturePath;
    }

    if (fs::exists(texturePath))
    {
        slot.resolvedPath = fs::canonical(texturePath);
    }

    return slot;
}
```

- [ ] **Step 3: Fill material factors and slots**

For each `aiMaterial`, fill:

```cpp
aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
if (AI_SUCCESS == aiGetMaterialColor(pMaterial, AI_MATKEY_BASE_COLOR, &baseColor))
{
    mat.baseColorFactor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
}

aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor))
{
    mat.emissiveFactor = glm::vec4(emissiveColor.r, emissiveColor.g, emissiveColor.b, 1.0f);
}

pMaterial->Get(AI_MATKEY_METALLIC_FACTOR, mat.metallicFactor);
pMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, mat.roughnessFactor);

mat.textures.baseColor = model_resolve_texture_slot(model, createInfo.filename, pMaterial, aiTextureType_BASE_COLOR, true);
if (!mat.textures.baseColor.valid())
{
    mat.textures.baseColor = model_resolve_texture_slot(model, createInfo.filename, pMaterial, aiTextureType_DIFFUSE, true);
}
mat.textures.normal = model_resolve_texture_slot(model, createInfo.filename, pMaterial, aiTextureType_NORMALS, false);
if (!mat.textures.normal.valid())
{
    mat.textures.normal = model_resolve_texture_slot(model, createInfo.filename, pMaterial, aiTextureType_NORMAL_CAMERA, false);
}
mat.textures.metallicRoughness = model_resolve_texture_slot(model, createInfo.filename, pMaterial, aiTextureType_METALNESS, false);
if (!mat.textures.metallicRoughness.valid())
{
    mat.textures.metallicRoughness = model_resolve_texture_slot(model, createInfo.filename, pMaterial, aiTextureType_DIFFUSE_ROUGHNESS, false);
}
mat.textures.emissive = model_resolve_texture_slot(model, createInfo.filename, pMaterial, aiTextureType_EMISSIVE, true);
if (!mat.textures.emissive.valid())
{
    mat.textures.emissive = model_resolve_texture_slot(model, createInfo.filename, pMaterial, aiTextureType_EMISSION_COLOR, true);
}
mat.textures.occlusion = model_resolve_texture_slot(model, createInfo.filename, pMaterial, aiTextureType_AMBIENT_OCCLUSION, false);
```

- [ ] **Step 4: Fix mesh part indexing**

When creating each part, store its material index:

```cpp
part.materialIndex = paiMesh->mMaterialIndex;
```

When appending faces, use `part.vertexBase`, not `part.indexBase`:

```cpp
model.indexData.push_back(part.vertexBase + Face.mIndices[0]);
model.indexData.push_back(part.vertexBase + Face.mIndices[1]);
model.indexData.push_back(part.vertexBase + Face.mIndices[2]);
```

- [ ] **Step 5: Verify CPU model test passes**

Run:

```powershell
.\build.bat
ctest --test-dir build -C Debug --output-on-failure -R vklive_model_tests_robot
```

Expected: `vklive_model_tests_robot` passes and reports no missing material slots or out-of-range indices.

---

### Task 3: Add Vulkan Material Resources

**Files:**
- Modify: `include/vklive/vulkan/vulkan_model.h`
- Modify: `src/vulkan/vulkan_model.cpp`

- [ ] **Step 1: Add GPU material structures**

In `include/vklive/vulkan/vulkan_model.h`, add:

```cpp
struct VulkanGpuMaterial
{
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    glm::vec4 emissiveFactor = glm::vec4(0.0f);
    glm::vec4 metallicRoughnessOcclusion = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    glm::ivec4 textureIndices = glm::ivec4(0);
};

struct VulkanModelTexture
{
    VulkanSurface surface{ nullptr };
    vk::DescriptorImageInfo descriptor;
    fs::path sourcePath;
    bool fallback = true;
};
```

Add to `VulkanModel`:

```cpp
VulkanBuffer materialsBuffer;
vk::DescriptorBufferInfo materialsDescriptor;
std::vector<VulkanGpuMaterial> gpuMaterials;
std::vector<VulkanModelTexture> baseColorTextures;
std::vector<VulkanModelTexture> normalTextures;
std::vector<VulkanModelTexture> metallicRoughnessTextures;
std::vector<VulkanModelTexture> emissiveTextures;
std::vector<VulkanModelTexture> occlusionTextures;
vk::DescriptorSet materialDescriptorSet = nullptr;
vk::DescriptorSetLayout materialDescriptorSetLayout = nullptr;
```

- [ ] **Step 2: Create fallback textures**

Add helper functions in `src/vulkan/vulkan_model.cpp`:

```cpp
VulkanModelTexture vulkan_model_create_solid_texture(VulkanContext& ctx, const std::string& debugName, const glm::u8vec4& color);
VulkanModelTexture vulkan_model_create_float_texture(VulkanContext& ctx, const std::string& debugName, const glm::vec4& color);
```

Use fallback colors:

- Base color: `(255, 255, 255, 255)`
- Normal: `(128, 128, 255, 255)`
- Metallic-roughness: `(0, 255, 0, 255)` because glTF stores roughness in `G` and metallic in `B`
- Emissive: `(0, 0, 0, 255)`
- Occlusion: `(255, 255, 255, 255)`

- [ ] **Step 3: Load material textures**

For each `ModelMaterial`, resolve each slot:

```cpp
VulkanModelTexture vulkan_model_load_texture_slot(VulkanContext& ctx, const ModelTextureSlot& slot, const VulkanModelTexture& fallback)
{
    if (!slot.resolvedPath.empty())
    {
        VulkanModelTexture texture;
        texture.surface.debugName = slot.resolvedPath.filename().string();
        if (surface_create_from_file(ctx, texture.surface, slot.resolvedPath))
        {
            texture.sourcePath = slot.resolvedPath;
            texture.fallback = false;
            texture.descriptor = vk::DescriptorImageInfo(texture.surface.sampler, texture.surface.view, vk::ImageLayout::eShaderReadOnlyOptimal);
            return texture;
        }
    }
    return fallback;
}
```

Embedded texture support should use `surface_create_from_memory` with the embedded byte data. If embedded upload fails, use the fallback.

- [ ] **Step 4: Upload material buffer**

Build `gpuMaterials` from CPU materials. For each material, copy factors and set texture array indices to the material index. Upload with:

```cpp
model.materialsBuffer = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eStorageBuffer, model.gpuMaterials);
model.materialsDescriptor = vk::DescriptorBufferInfo(model.materialsBuffer.buffer, 0, VK_WHOLE_SIZE);
```

- [ ] **Step 5: Destroy material resources**

In `vulkan_model_destroy`, destroy `materialsBuffer` and each material texture surface when `refCount` drops to zero.

---

### Task 4: Bind Standard Material Descriptor Set

**Files:**
- Modify: `src/vulkan/vulkan_pass.cpp`
- Modify: `src/vulkan/vulkan_pipeline.cpp`

- [ ] **Step 1: Support descriptor arrays**

In `vulkan_pass_set_descriptors`, when writing descriptors, use:

```cpp
newWrite.descriptorCount = binding.descriptorCount;
```

For image arrays, keep a `std::vector<std::vector<vk::DescriptorImageInfo>> imageArrayInfos;` alive until `ctx.device.updateDescriptorSets(...)` runs.

- [ ] **Step 2: Reserve material set**

Do not allocate set 2 in the generic pass descriptor path. Store its layout in `passFrameData.descriptorSetLayouts[2]` so each `VulkanModel` can allocate a matching material descriptor set.

Use names from `vklive_pbr_material.glsl`:

- `vklMaterials`
- `vklBaseColorTextures`
- `vklNormalTextures`
- `vklMetallicRoughnessTextures`
- `vklEmissiveTextures`
- `vklOcclusionTextures`

- [ ] **Step 3: Allocate per-model material descriptor set**

Add helper:

```cpp
void vulkan_model_prepare_material_descriptors(VulkanContext& ctx, VulkanModel& model, vk::DescriptorSetLayout layout);
```

It writes:

- set 2 binding 0: `model.materialsDescriptor`
- set 2 binding 1: `model.baseColorTextures` descriptors
- set 2 binding 2: `model.normalTextures` descriptors
- set 2 binding 3: `model.metallicRoughnessTextures` descriptors
- set 2 binding 4: `model.emissiveTextures` descriptors
- set 2 binding 5: `model.occlusionTextures` descriptors

If a model has fewer than `VKLIVE_MAX_MATERIALS`, repeat fallback descriptors for the remaining entries.

- [ ] **Step 4: Add push constant range**

Create a shared C++ structure:

```cpp
struct VklDrawPushConstants
{
    uint32_t materialIndex = 0;
};
```

When creating graphics pipeline layouts, add:

```cpp
vk::PushConstantRange drawPushConstants(
    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
    0,
    sizeof(VklDrawPushConstants));
```

The range can be present even when a shader does not declare the push constant.

- [ ] **Step 5: Draw each model part**

Replace the single draw call:

```cpp
cmd.drawIndexed(pVulkanGeom->indexCount, 1, 0, 0, 0);
```

with:

```cpp
for (const auto& part : pVulkanGeom->parts)
{
    VklDrawPushConstants constants;
    constants.materialIndex = part.materialIndex;
    cmd.pushConstants(passFrameData.geometryPipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(constants), &constants);
    cmd.drawIndexed(part.indexCount, 1, part.indexBase, 0, 0);
}
```

Before each model, bind set 2 if the shader declared the material set:

```cpp
if (pVulkanGeom->materialDescriptorSet)
{
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, passFrameData.geometryPipelineLayout, 2, pVulkanGeom->materialDescriptorSet, {});
}
```

---

### Task 5: Add Standard GLSL Material Include and Update Sample

**Files:**
- Create: `run_tree/shaders/include/vklive_pbr_material.glsl`
- Modify: `run_tree/projects/pbr_robot/default.scenegraph`
- Modify: `run_tree/projects/pbr_robot/pbr.frag`
- Modify: `tests/test_pbr_template.py`

- [ ] **Step 1: Add include file**

Create `run_tree/shaders/include/vklive_pbr_material.glsl` using the contract listed in the "Standard Shader Interface" section.

- [ ] **Step 2: Remove body-part scene samplers**

In `run_tree/projects/pbr_robot/default.scenegraph`, remove:

```txt
surface: RobotChest_baseColor
surface: RobotHead_baseColor
surface: RobotExtremities_baseColor
```

and change the robot pass to:

```txt
samplers: (studio_sky)
```

- [ ] **Step 3: Use material helpers in `pbr.frag`**

Use:

```glsl
#include "vklive_pbr_material.glsl"

vec4 baseColor = vklBaseColor(outUV);
vec2 metallicRoughness = vklMetallicRoughness(outUV);
vec3 emissive = vklEmissive(outUV);
float occlusion = vklOcclusion(outUV);
```

Then feed:

- `baseColor.rgb` into albedo
- `metallicRoughness.x` into metallic
- `metallicRoughness.y` into roughness
- `emissive` into final color
- `occlusion` into ambient/environment diffuse

- [ ] **Step 4: Update template tests**

In `tests/test_pbr_template.py`, assert:

```python
self.assertIn('#include "vklive_pbr_material.glsl"', pbr)
self.assertIn("vklBaseColor(outUV)", pbr)
self.assertIn("vklMetallicRoughness(outUV)", pbr)
self.assertNotIn("RobotChest_baseColor", pbr)
self.assertNotIn("RobotHead_baseColor", pbr)
self.assertNotIn("RobotExtremities_baseColor", pbr)
```

- [ ] **Step 5: Verify shader compile**

Run:

```powershell
$validator = Resolve-Path run_tree\bin\win\glslangValidator.exe
$incProject = Resolve-Path run_tree\projects\pbr_robot
$incShared = Resolve-Path run_tree\shaders\include
& $validator -V run_tree\projects\pbr_robot\pbr.frag -o $env:TEMP\pbr.frag.spv -l -g "-I$incProject" "-I$incShared"
```

Expected: `pbr.frag` compiles with set 1 `studio_sky` and set 2 material bindings.

---

### Task 6: Fault Tolerance and Hot Reload Behavior

**Files:**
- Modify: `src/vulkan/vulkan_model.cpp`
- Modify: `src/vulkan/vulkan_pass.cpp`

- [ ] **Step 1: Material texture failure policy**

If a texture path is missing or upload fails:

```cpp
scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not load material texture: {}", slot.pathName));
```

Then bind the fallback texture for that material slot and continue rendering.

- [ ] **Step 2: Material descriptor failure policy**

If descriptor allocation fails for set 2, report an error and draw the model with fallback material textures rather than skipping the whole pass.

- [ ] **Step 3: Shader compatibility policy**

Shaders that do not include `vklive_pbr_material.glsl` continue working. The engine only prepares and binds set 2 when reflection finds set 2 material bindings.

- [ ] **Step 4: Multi-model pass policy**

Each model owns its set 2 material descriptor set. The pass binds set 0 and set 1 once, then binds set 2 per model before drawing its parts.

---

### Task 7: Final Verification

**Files:**
- All changed files.

- [ ] **Step 1: Configure**

Run:

```powershell
.\config.bat
```

Expected: exit code `0`.

- [ ] **Step 2: Build Debug**

Run:

```powershell
.\build.bat
```

Expected: exit code `0`.

- [ ] **Step 3: Run tests**

Run:

```powershell
ctest --test-dir build -C Debug --output-on-failure -R vklive_model_tests_robot
python -m unittest tests.test_do tests.test_pbr_template tests.test_surface_hdr_static
```

Expected: all tests pass.

- [ ] **Step 4: Compile sample shaders**

Run the `glslangValidator` command from Task 5 for `skybox.vert`, `skybox.frag`, `pbr.vert`, and `pbr.frag`.

Expected: all four shaders compile.

- [ ] **Step 5: Manual visual verification**

Run:

```powershell
python do.py run release
```

Open the `pbr_robot` template. Expected visual result:

- The robot has three visible material regions.
- Base color textures match the glTF material assignments.
- Metallic-roughness affects reflectivity without turning the whole robot into the sky color.
- Emissive areas contribute visible glow/color where present.
- The robot slowly rotates and all sections remain visible from every side.

---

## Notes for Implementation

- Keep `set = 1` as the user-facing scene sampler space. Do not put glTF material textures there.
- Keep material names generic and type-based. Users should write `vklBaseColor(outUV)`, not `RobotChest_baseColor`.
- Do not require descriptor set 2 for legacy shaders.
- Fix CPU index generation before working on material descriptors; otherwise rendering correctness remains ambiguous.
- Use fallback textures everywhere a material texture can fail. This preserves the app's fault-tolerant editing model.
