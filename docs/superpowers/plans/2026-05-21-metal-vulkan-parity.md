# Metal Vulkan Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the native Metal renderer to practical feature parity with the current Vulkan renderer while preserving Vulkan behavior on Windows and Linux.

**Architecture:** Implement parity as vertical renderer subsystems behind the existing `IDevice` boundary and the existing `MetalContext`/`MetalScene`/`MetalPass` split. Start with resource binding, texture sampling, multiple render targets, and material descriptors because those unlock most current sample projects; then add capture, scripted passes, viewports, ray tracing, and geometry-stage parity as separate verified milestones.

**Tech Stack:** C++20, Objective-C++ `.mm`, Apple Metal/MetalKit/QuartzCore, SDL2 Metal views, Dear ImGui Metal backend, SPIRV-Reflect, SPIRV-Cross MSL, Zing audio analysis, lodepng, CMake, CTest.

---

## Starting State

- Metal standard passes compile `.vert` and `.frag` shaders through GLSL -> SPIR-V -> MSL.
- Metal pass drawing only binds the default UBO at `set = 0, binding = 0`.
- Metal rejects pass samplers, non-standard pass types, geometry shader groups, ray shader groups, and more than one color target.
- Metal model staging uploads vertex and index buffers only; it does not expose storage-buffer descriptors, material buffers, material textures, or acceleration structures.
- Metal recording is stubbed and disables scene recording.
- Metal ImGui multi-viewports are disabled.
- `src/metal/metal_imgui.mm` has a pending local startup-crash fix that should be committed separately before parity work begins.

## Implementation Order

The work should land in this order because each milestone unlocks real sample coverage and creates primitives needed by later milestones:

1. Shared parity test harness and sample matrix.
2. Metal resource binding layer.
3. Metal sampled surfaces, texture files, audio surfaces, and ping-pong target sampling.
4. Metal multiple render targets.
5. Metal model materials and PBR texture descriptors.
6. Metal PNG recording/capture.
7. Metal scripted pass support.
8. Metal ImGui multi-viewports.
9. Metal ray tracing equivalent.
10. Geometry shader compatibility strategy.

## Feature Parity Acceptance Matrix

Each row must have an automated or scripted verification command before the corresponding milestone is marked complete.

| Area | Vulkan behavior | Metal target behavior | Verification |
| --- | --- | --- | --- |
| Standard raster | Vertex+fragment raster passes draw models | Same output path and sample compatibility | `run_tree/projects/simple` and `run_tree/projects/pbr_robot` startup-frame checks |
| Resource bindings | UBO, sampled image, storage image, storage buffer, acceleration structure descriptors | Metal slot mapping and bind calls for the same reflected resources | New `vklive_metal_binding_tests` plus sample runs |
| Samplers | File textures, render targets, alternate ping-pong, audio analysis texture | `MTLTexture`/`MTLSamplerState` creation and per-pass binding | `run_tree/projects/default`, `blend_waves`, `audio_spectrum_analysis` |
| MRT | Multiple color attachments plus optional depth | Multiple `colorAttachments[]` in Metal pass descriptors and pipeline state | `run_tree/projects/deferred_shading` |
| Materials | Material storage buffer plus texture arrays at set 2 | Metal material buffer plus texture arrays and material index argument | `run_tree/projects/pbr_robot` |
| Capture | Writes PNG frames while recording | Read back Metal output texture and encode PNG | Recording smoke command writes `Frame_00001.png` |
| Scripted pass | Python/NanoVG pass renders into Vulkan target | Metal NanoVG or equivalent 2D backend renders into Metal target | Scripted scene fixture and image checksum |
| Viewports | ImGui platform windows render with Vulkan backend | ImGui platform windows render with Metal backend | Manual and startup-frame viewport smoke |
| Ray tracing | Vulkan KHR ray pipeline, AS, SBT, storage image target | Metal acceleration structures and ray dispatch for supported scenes | `run_tree/projects/ray_tracer` |
| Geometry shader | Vulkan `.geom` stage works | Metal-compatible equivalent for current geometry-shader samples | `run_tree/projects/default` and `protoplanetary_disc` |

---

### Task 1: Add Renderer Parity Harness

**Files:**
- Create: `tests/render_parity_harness.cpp`
- Create: `tests/content/render_parity/README.md`
- Modify: `CMakeLists.txt`
- Modify: `app/include/app/command_line.h`
- Modify: `app/src/command_line.cpp`
- Modify: `app/src/main.cpp`
- Test: `tests/app_command_line_tests.cpp`

- [ ] **Step 1: Add a startup-frame command-line mode**

Add `startupFrameTest` to `AppCommandLineOptions` in `app/include/app/command_line.h`:

```cpp
bool startupFrameTest = false;
```

Add parsing for `--startup-frame-test` in `app/src/command_line.cpp`:

```cpp
else if (arg == "--startup-frame-test")
{
    options.startupFrameTest = true;
}
```

Add this line to the help text:

```text
  --startup-frame-test            Initialize the renderer, draw one app frame, then exit.
```

In `app/src/main.cpp`, after `g_pDevice->Present();`, exit the main loop when this flag is set:

```cpp
if (commandLineOptions.startupFrameTest)
{
    done = true;
}
```

- [ ] **Step 2: Test command-line parsing**

Append this case to `tests/app_command_line_tests.cpp`:

```cpp
const char* startupFrameArgv[] = {
    "Rezonality.exe",
    "--renderer",
    "metal",
    "--startup-frame-test"
};
AppCommandLineOptions startupFrameOptions;
std::string startupFrameError;
bool startupFrameOk = app_parse_command_line(static_cast<int>(std::size(startupFrameArgv)), const_cast<char**>(startupFrameArgv), startupFrameOptions, startupFrameError);
ok &= require(startupFrameOk, "startup-frame parser failed: " + startupFrameError);
ok &= require(startupFrameOptions.renderer == RenderBackend::Metal, "startup-frame renderer not parsed");
ok &= require(startupFrameOptions.startupFrameTest, "startup-frame flag not parsed");
```

Run:

```sh
cmake --build build --config Debug --target vklive_app_command_line_tests
ctest --test-dir build --output-on-failure -R vklive_app_command_line_tests
```

Expected: command-line test passes.

- [ ] **Step 3: Add parity harness executable**

Create `tests/render_parity_harness.cpp`:

```cpp
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: render_parity_harness <Rezonality> <project> [renderer]\n";
        return EXIT_FAILURE;
    }

    const fs::path app = argv[1];
    const fs::path project = argv[2];
    const std::string renderer = argc >= 4 ? argv[3] : "metal";

    if (!fs::exists(app))
    {
        std::cerr << "missing app executable: " << app << "\n";
        return EXIT_FAILURE;
    }
    if (!fs::exists(project))
    {
        std::cerr << "missing project: " << project << "\n";
        return EXIT_FAILURE;
    }

    const std::string command =
        "\"" + app.string() + "\" --renderer " + renderer +
        " --project \"" + project.string() + "\" --startup-frame-test";

    const int rc = std::system(command.c_str());
    if (rc != 0)
    {
        std::cerr << "render parity command failed: " << command << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
```

Add it to `CMakeLists.txt` as a CTest target:

```cmake
add_executable(vklive_render_parity_harness tests/render_parity_harness.cpp)
add_test(NAME vklive_render_parity_simple_metal
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/run_tree/projects/simple
        metal)
```

Run:

```sh
cmake --build build --config Debug --target vklive_render_parity_harness Rezonality
ctest --test-dir build --output-on-failure -R vklive_render_parity_simple_metal
```

Expected: simple project starts, draws one frame, and exits successfully on Metal.

- [ ] **Step 4: Commit**

```sh
git add app/include/app/command_line.h app/src/command_line.cpp app/src/main.cpp tests/app_command_line_tests.cpp tests/render_parity_harness.cpp tests/content/render_parity/README.md CMakeLists.txt
git commit -m "test: add renderer startup parity harness"
```

---

### Task 2: Implement Metal Resource Binding Layer

**Files:**
- Modify: `include/vklive/metal/metal_shader.h`
- Modify: `src/metal/metal_shader.mm`
- Modify: `include/vklive/metal/metal_pass.h`
- Modify: `src/metal/metal_pass.mm`
- Create: `tests/metal_binding_tests.mm`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Extend reflected Metal binding metadata**

Update `MetalShaderResourceBinding` in `include/vklive/metal/metal_shader.h`:

```cpp
struct MetalShaderResourceBinding
{
    uint32_t set = 0;
    uint32_t binding = 0;
    ShaderBindingType type = ShaderBindingType::Unknown;
    uint32_t count = 1;
    uint32_t bufferIndex = std::numeric_limits<uint32_t>::max();
    uint32_t textureIndex = std::numeric_limits<uint32_t>::max();
    uint32_t samplerIndex = std::numeric_limits<uint32_t>::max();
};
```

In `src/metal/metal_shader.mm`, copy descriptor array counts during `assign_resource_bindings`:

```cpp
metalBinding.count = reflectedBinding.count;
```

- [ ] **Step 2: Replace the basic-pass binding gate**

Remove `binding_supported_for_basic_pass` and `metal_shader_bindings_supported` from `src/metal/metal_pass.mm`. Keep validation for unsupported Metal concepts in bind-time code, where the renderer can report the exact missing resource name and shader location.

Update `metal_pass_get_shaders` so it no longer rejects any binding type during shader selection.

- [ ] **Step 3: Add reusable bind helpers**

Add these private helpers to `src/metal/metal_pass.mm`:

```objc
void metal_pass_set_buffer(id<MTLRenderCommandEncoder> encoder, const metal::MetalShader& shader, const metal::MetalShaderResourceBinding& binding, id<MTLBuffer> buffer)
{
    if (!buffer || binding.bufferIndex == std::numeric_limits<uint32_t>::max())
    {
        return;
    }
    if (shader.stage == metal::MetalShaderStage::Vertex)
    {
        [encoder setVertexBuffer:buffer offset:0 atIndex:binding.bufferIndex];
    }
    else
    {
        [encoder setFragmentBuffer:buffer offset:0 atIndex:binding.bufferIndex];
    }
}

void metal_pass_set_texture(id<MTLRenderCommandEncoder> encoder, const metal::MetalShader& shader, const metal::MetalShaderResourceBinding& binding, id<MTLTexture> texture)
{
    if (!texture || binding.textureIndex == std::numeric_limits<uint32_t>::max())
    {
        return;
    }
    if (shader.stage == metal::MetalShaderStage::Vertex)
    {
        [encoder setVertexTexture:texture atIndex:binding.textureIndex];
    }
    else
    {
        [encoder setFragmentTexture:texture atIndex:binding.textureIndex];
    }
}

void metal_pass_set_sampler(id<MTLRenderCommandEncoder> encoder, const metal::MetalShader& shader, const metal::MetalShaderResourceBinding& binding, id<MTLSamplerState> sampler)
{
    if (!sampler || binding.samplerIndex == std::numeric_limits<uint32_t>::max())
    {
        return;
    }
    if (shader.stage == metal::MetalShaderStage::Vertex)
    {
        [encoder setVertexSamplerState:sampler atIndex:binding.samplerIndex];
    }
    else
    {
        [encoder setFragmentSamplerState:sampler atIndex:binding.samplerIndex];
    }
}
```

- [ ] **Step 4: Keep UBO binding working through the new helper**

Replace `metal_pass_bind_uniform` with:

```objc
void metal_pass_bind_uniform(id<MTLRenderCommandEncoder> encoder, const metal::MetalShader& shader, id<MTLBuffer> uniformBuffer)
{
    auto itrBinding = shader.resourceBindings.find({ 0, 0 });
    if (itrBinding == shader.resourceBindings.end())
    {
        return;
    }
    metal_pass_set_buffer(encoder, shader, itrBinding->second, uniformBuffer);
}
```

Run:

```sh
cmake --build build --config Debug
ctest --test-dir build --output-on-failure
```

Expected: existing tests pass and the simple Metal startup-frame test still passes.

- [ ] **Step 5: Add binding-index unit coverage**

Create `tests/metal_binding_tests.mm` with a small fixture that calls the reflection path on a fragment shader containing:

```glsl
layout(set = 0, binding = 0) uniform UBO { float iTime; } ubo;
layout(set = 1, binding = 0) uniform sampler2D texA;
layout(set = 1, binding = 1) uniform sampler2D texB;
```

Assert:

```cpp
ok &= require(shader.resourceBindings[{0, 0}].bufferIndex == 1, "UBO buffer index should follow vertex buffer slot");
ok &= require(shader.resourceBindings[{1, 0}].textureIndex == 0, "texA texture index mismatch");
ok &= require(shader.resourceBindings[{1, 0}].samplerIndex == 0, "texA sampler index mismatch");
ok &= require(shader.resourceBindings[{1, 1}].textureIndex == 1, "texB texture index mismatch");
ok &= require(shader.resourceBindings[{1, 1}].samplerIndex == 1, "texB sampler index mismatch");
```

Run:

```sh
cmake --build build --config Debug --target vklive_metal_binding_tests
ctest --test-dir build --output-on-failure -R vklive_metal_binding_tests
```

Expected: Metal binding tests pass.

- [ ] **Step 6: Commit**

```sh
git add include/vklive/metal/metal_shader.h src/metal/metal_shader.mm include/vklive/metal/metal_pass.h src/metal/metal_pass.mm tests/metal_binding_tests.mm CMakeLists.txt
git commit -m "feat: add Metal resource binding layer"
```

---

### Task 3: Implement Metal Sampled Surfaces

**Files:**
- Modify: `include/vklive/metal/metal_surface.h`
- Modify: `src/metal/metal_surface.mm`
- Modify: `include/vklive/metal/metal_scene.h`
- Modify: `src/metal/metal_scene.mm`
- Modify: `src/metal/metal_pass.mm`
- Test: `tests/render_parity_harness.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add texture loading state to `MetalSurface`**

Add to `MetalSurface` in `include/vklive/metal/metal_surface.h`:

```cpp
enum class MetalAllocationState
{
    Init,
    Loaded,
    Failed
};

MetalAllocationState allocationState = MetalAllocationState::Init;
uint32_t mipLevels = 1;
bool isAudioSurface = false;
void* stagingBuffer = nullptr;
```

- [ ] **Step 2: Add Metal texture loading functions**

Declare in `include/vklive/metal/metal_surface.h`:

```cpp
bool metal_surface_create_from_file(MetalContext& ctx, MetalSurface& surface, const fs::path& path, bool flipY = false);
bool metal_surface_create_from_memory(MetalContext& ctx, MetalSurface& surface, const fs::path& sourceName, const char* data, size_t dataSize, bool flipY = false);
bool metal_surface_update_from_audio(MetalContext& ctx, MetalSurface& surface);
```

Implement both file and memory loading in `src/metal/metal_surface.mm` using `stb_image` for LDR/HDR image data and `gli` for `.dds`/`.ktx`, mirroring `surface_create_from_memory` in `src/vulkan/vulkan_surface.cpp`. Use `MTLPixelFormatRGBA8Unorm`, `MTLPixelFormatRGBA8Unorm_sRGB`, or `MTLPixelFormatRGBA32Float` according to the loaded data.

- [ ] **Step 3: Bind sampled surfaces by reflected shader name**

Add a map builder to `metal_pass_encode_draw` in `src/metal/metal_pass.mm`:

```objc
std::map<std::string, metal::MetalSurface*> sampledSurfaces;
for (auto& passSampler : pass.pass.samplers)
{
    auto* surface = metal::metal_scene_get_or_create_surface(ctx, pass.metalScene, passSampler.sampler, Scene::GlobalFrameCount, passSampler.sampleAlternate);
    if (surface)
    {
        sampledSurfaces[passSampler.sampler] = surface;
    }
}
```

For each shader resource binding with `CombinedImageSampler`, `SampledImage`, or `Sampler`, find the binding metadata name and bind:

```objc
auto itrSurface = sampledSurfaces.find(bindingName);
if (itrSurface != sampledSurfaces.end())
{
    auto* surface = itrSurface->second;
    metal_pass_set_texture(encoder, shader, metalBinding, bridge<id<MTLTexture>>(surface->texture));
    metal_pass_set_sampler(encoder, shader, metalBinding, bridge<id<MTLSamplerState>>(surface->sampler));
}
```

- [ ] **Step 4: Create or update sampled surfaces before drawing**

Add `metal_pass_prepare_samplers` before pipeline creation:

```cpp
bool metal_pass_prepare_samplers(metal::MetalContext& ctx, metal::MetalPass& pass)
{
    bool ok = true;
    for (auto& passSampler : pass.pass.samplers)
    {
        auto* metalSurface = metal::metal_scene_get_or_create_surface(ctx, pass.metalScene, passSampler.sampler, Scene::GlobalFrameCount, passSampler.sampleAlternate);
        if (!metalSurface || !metalSurface->pSurface)
        {
            ok = false;
            continue;
        }
        if (!metalSurface->pSurface->isTarget && metalSurface->allocationState == metal::MetalAllocationState::Init)
        {
            if (metalSurface->pSurface->name == "AudioAnalysis")
            {
                ok &= metal::metal_surface_update_from_audio(ctx, *metalSurface);
            }
            else if (!metalSurface->pSurface->path.empty())
            {
                auto file = scene_find_asset(pass.pass.scene, metalSurface->pSurface->path, AssetType::Texture);
                ok &= !file.empty() && metal::metal_surface_create_from_file(ctx, *metalSurface, file);
            }
        }
        if (!metalSurface->sampler && metalSurface->texture)
        {
            metal::metal_surface_create_sampler(ctx, *metalSurface);
        }
    }
    return ok;
}
```

Call it in `metal_pass_draw` before `metal_pass_ensure_pipeline`.

- [ ] **Step 5: Verify texture and feedback samples**

Add CTest entries:

```cmake
add_test(NAME vklive_render_parity_default_metal
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/run_tree/projects/default
        metal)

add_test(NAME vklive_render_parity_audio_metal
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/run_tree/projects/shadertoy/audio_spectrum_analysis
        metal)
```

Run:

```sh
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -R "vklive_render_parity_(default|audio)_metal"
```

Expected: sampled texture projects render one startup frame without Metal validation or scene errors.

- [ ] **Step 6: Commit**

```sh
git add include/vklive/metal/metal_surface.h src/metal/metal_surface.mm include/vklive/metal/metal_scene.h src/metal/metal_scene.mm src/metal/metal_pass.mm tests/render_parity_harness.cpp CMakeLists.txt
git commit -m "feat: support sampled surfaces in Metal"
```

---

### Task 4: Implement Multiple Render Targets

**Files:**
- Modify: `include/vklive/metal/metal_pass.h`
- Modify: `src/metal/metal_pass.mm`
- Modify: `src/metal/metal_scene.mm`
- Test: `tests/render_parity_harness.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Replace single color target state**

Change `MetalPassTargets` in `src/metal/metal_pass.mm` from one `color` pointer to:

```cpp
std::vector<metal::MetalSurface*> colors;
std::vector<MTLPixelFormat> colorFormats;
metal::MetalSurface* depth = nullptr;
```

Change `MetalPass` in `include/vklive/metal/metal_pass.h` from `colorTargetKey` and `colorPixelFormat` to:

```cpp
std::vector<MetalSurfaceKey> colorTargetKeys;
std::vector<uint64_t> colorTargetGenerations;
std::vector<uint32_t> colorPixelFormats;
```

- [ ] **Step 2: Accept multiple color targets**

In `metal_pass_prepare_targets`, remove the rejection for a second color target and append each color target:

```cpp
targets.colors.push_back(pMetalSurface);
targets.colorFormats.push_back(pixelFormat);
```

Set `targets.size` from the first color target and validate every other target matches it.

- [ ] **Step 3: Create pipeline color attachments for every color target**

In `metal_pass_ensure_pipeline`, set:

```objc
for (NSUInteger i = 0; i < targets.colorFormats.size(); ++i)
{
    descriptor.colorAttachments[i].pixelFormat = targets.colorFormats[i];
    descriptor.colorAttachments[i].blendingEnabled = YES;
    descriptor.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    descriptor.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    descriptor.colorAttachments[i].rgbBlendOperation = MTLBlendOperationAdd;
    descriptor.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorOne;
    descriptor.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    descriptor.colorAttachments[i].alphaBlendOperation = MTLBlendOperationAdd;
}
```

- [ ] **Step 4: Encode all color attachments**

In `metal_pass_encode_draw`, bind each target:

```objc
for (NSUInteger i = 0; i < targets.colors.size(); ++i)
{
    renderPass.colorAttachments[i].texture = bridge<id<MTLTexture>>(targets.colors[i]->texture);
    renderPass.colorAttachments[i].loadAction = pass.pass.hasClear ? MTLLoadActionClear : MTLLoadActionLoad;
    renderPass.colorAttachments[i].storeAction = MTLStoreActionStore;
    renderPass.colorAttachments[i].clearColor = MTLClearColorMake(pass.pass.clearColor.x, pass.pass.clearColor.y, pass.pass.clearColor.z, pass.pass.clearColor.w);
}
```

Mark every color target as rendered after command buffer completion.

- [ ] **Step 5: Verify deferred shading**

Add:

```cmake
add_test(NAME vklive_render_parity_deferred_metal
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/run_tree/projects/deferred_shading
        metal)
```

Run:

```sh
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -R vklive_render_parity_deferred_metal
```

Expected: Metal accepts the deferred shading scene and exposes `Positions`, `Albedo`, and `Normals` as target views.

- [ ] **Step 6: Commit**

```sh
git add include/vklive/metal/metal_pass.h src/metal/metal_pass.mm src/metal/metal_scene.mm CMakeLists.txt
git commit -m "feat: support Metal multiple render targets"
```

---

### Task 5: Implement Metal Model Materials

**Files:**
- Modify: `include/vklive/metal/metal_model.h`
- Modify: `src/metal/metal_model.mm`
- Modify: `src/metal/metal_pass.mm`
- Modify: `run_tree/shaders/include/vklive_pbr_material.glsl`
- Test: `tests/render_parity_harness.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Mirror Vulkan material GPU data**

Add to `include/vklive/metal/metal_model.h`:

```cpp
inline constexpr uint32_t MetalMaxMaterials = 64;

struct MetalGpuMaterial
{
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    glm::vec4 emissiveFactor = glm::vec4(0.0f);
    glm::vec4 metallicRoughnessOcclusion = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    glm::ivec4 textureIndices = glm::ivec4(0);
};

struct MetalModelTexture
{
    void* texture = nullptr;
    void* sampler = nullptr;
    fs::path sourcePath;
    bool fallback = true;
};
```

Add fields to `MetalModel`:

```cpp
void* materialsBuffer = nullptr;
std::vector<MetalGpuMaterial> gpuMaterials;
std::vector<MetalModelTexture> materialTextures;
std::vector<MetalModelTexture> baseColorTextures;
std::vector<MetalModelTexture> normalTextures;
std::vector<MetalModelTexture> metallicRoughnessTextures;
std::vector<MetalModelTexture> emissiveTextures;
std::vector<MetalModelTexture> occlusionTextures;
```

- [ ] **Step 2: Load material textures**

In `src/metal/metal_model.mm`, add helpers equivalent to Vulkan fallback texture creation and texture-slot loading:

```objc
MetalModelTexture metal_model_create_solid_texture(MetalContext& ctx, const std::string& debugName, const std::array<uint8_t, 4>& rgba);
MetalModelTexture metal_model_load_texture_slot(MetalContext& ctx, MetalModel& model, const ModelTextureSlot& slot, const MetalModelTexture& fallback, const std::string& debugPrefix);
```

Use `metal_surface_create_from_memory` for embedded textures and `metal_surface_create_from_file` for file textures, then retain the resulting `MTLTexture` and `MTLSamplerState` in `MetalModelTexture`.

- [ ] **Step 3: Create material buffer**

After vertex/index staging in `metal_model_stage`, build `gpuMaterials` and create:

```objc
model.materialsBuffer = (__bridge_retained void*)[device newBufferWithBytes:model.gpuMaterials.data()
                                                                      length:model.gpuMaterials.size() * sizeof(MetalGpuMaterial)
                                                                     options:MTLResourceStorageModeShared];
```

- [ ] **Step 4: Bind material set 2**

In `src/metal/metal_pass.mm`, when drawing each model part:

```objc
VklDrawPushConstants constants;
constants.materialIndex = part.materialIndex;
[encoder setVertexBytes:&constants length:sizeof(constants) atIndex:0];
[encoder setFragmentBytes:&constants length:sizeof(constants) atIndex:0];
```

Bind set 2 resources by reflected names:

```objc
// vklMaterials -> model.materialsBuffer
// vklBaseColorTextures -> model.baseColorTextures
// vklNormalTextures -> model.normalTextures
// vklMetallicRoughnessTextures -> model.metallicRoughnessTextures
// vklEmissiveTextures -> model.emissiveTextures
// vklOcclusionTextures -> model.occlusionTextures
```

For texture arrays, call `setFragmentTextures:withRange:` and `setFragmentSamplerStates:withRange:` using arrays sized to `MetalMaxMaterials`.

- [ ] **Step 5: Verify PBR robot**

Run:

```sh
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -R vklive_render_parity_pbr_robot_metal
```

Add the CTest entry if it does not exist:

```cmake
add_test(NAME vklive_render_parity_pbr_robot_metal
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/run_tree/projects/pbr_robot
        metal)
```

Expected: PBR robot startup-frame render completes without missing descriptor errors.

- [ ] **Step 6: Commit**

```sh
git add include/vklive/metal/metal_model.h src/metal/metal_model.mm src/metal/metal_pass.mm run_tree/shaders/include/vklive_pbr_material.glsl CMakeLists.txt
git commit -m "feat: support Metal model materials"
```

---

### Task 6: Implement Metal PNG Recording

**Files:**
- Modify: `include/vklive/metal/metal_surface.h`
- Modify: `src/metal/metal_surface.mm`
- Modify: `src/metal/metal_scene.mm`
- Test: `tests/render_parity_harness.cpp`

- [ ] **Step 1: Add readback helper**

Declare:

```cpp
bool metal_surface_read_rgba8(MetalContext& ctx, MetalSurface& surface, std::vector<uint8_t>& pixels, glm::uvec2& size);
```

Implement with a `MTLBuffer` blit readback:

```objc
id<MTLCommandQueue> queue = bridge<id<MTLCommandQueue>>(ctx.commandQueue);
id<MTLTexture> texture = bridge<id<MTLTexture>>(surface.texture);
id<MTLBuffer> readback = [device newBufferWithLength:bytesPerRow * size.y options:MTLResourceStorageModeShared];
id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
[blit copyFromTexture:texture
          sourceSlice:0
          sourceLevel:0
         sourceOrigin:MTLOriginMake(0, 0, 0)
           sourceSize:MTLSizeMake(size.x, size.y, 1)
             toBuffer:readback
    destinationOffset:0
destinationBytesPerRow:bytesPerRow
destinationBytesPerImage:bytesPerRow * size.y];
[blit endEncoding];
[commandBuffer commit];
[commandBuffer waitUntilCompleted];
```

- [ ] **Step 2: Encode PNG frames**

Replace `metal_scene_write_to_file` with readback plus `lodepng::encode`:

```cpp
auto itrTarget = metalScene.surfaces.find(metalScene.defaultTarget);
if (itrTarget == metalScene.surfaces.end() || !itrTarget->second)
{
    metalScene.pScene->recording = false;
    return;
}

std::vector<uint8_t> rgba;
glm::uvec2 size;
if (!metal_surface_read_rgba8(ctx, *itrTarget->second, rgba, size))
{
    metalScene.pScene->recording = false;
    return;
}

fs::create_directories(path);
auto fileName = path / fmt::format("Frame_{:05}.png", Scene::GlobalFrameCount);
lodepng::encode(fileName.string(), rgba, size.x, size.y, LCT_RGBA);
```

- [ ] **Step 3: Verify recording output**

Run a recording smoke command through a small fixture that sets `scene.recording = true` for one frame, or add a command-line option:

```sh
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/simple --startup-frame-test --record-one-frame
test -f run_tree/renders/Frame_00001.png
```

Expected: `Frame_00001.png` exists and has non-zero size.

- [ ] **Step 4: Commit**

```sh
git add include/vklive/metal/metal_surface.h src/metal/metal_surface.mm src/metal/metal_scene.mm tests/render_parity_harness.cpp
git commit -m "feat: add Metal PNG recording"
```

---

### Task 7: Implement Metal Scripted Passes

**Files:**
- Create: `include/vklive/metal/metal_nanovg.h`
- Create: `src/metal/metal_nanovg.mm`
- Modify: `include/vklive/metal/metal_context.h`
- Modify: `src/metal/metal_device.mm`
- Modify: `src/metal/metal_pass.mm`
- Modify: `src/python_scripting.cpp` if the NanoVG backend needs a shared initialization hook
- Test: `tests/render_parity_harness.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add a Metal NanoVG context wrapper**

Create `include/vklive/metal/metal_nanovg.h`:

```cpp
#pragma once

#include <glm/glm.hpp>

struct NVGcontext;
struct Pass;

namespace metal
{
struct MetalContext;
struct MetalPass;
struct MetalPassTargets;

bool metal_nanovg_init(MetalContext& ctx);
void metal_nanovg_destroy(MetalContext& ctx);
bool metal_nanovg_begin(MetalContext& ctx, MetalPass& pass, const glm::uvec2& targetSize);
void metal_nanovg_end(MetalContext& ctx);
}
```

Add to `MetalContext`:

```cpp
NVGcontext* vg = nullptr;
```

- [ ] **Step 2: Initialize and destroy from `MetalDevice`**

In `MetalDevice::MetalDevice`, after `imgui_init`:

```cpp
metal_nanovg_init(ctx);
```

In `MetalDevice::~MetalDevice`, before `context_destroy`:

```cpp
metal_nanovg_destroy(ctx);
```

- [ ] **Step 3: Add scripted branch in `metal_pass_draw`**

Permit `PassType::Scripted` in Metal validation. In `metal_pass_draw`, branch:

```cpp
if (pass.pass.passType == PassType::Scripted)
{
    MetalPassTargets targets;
    if (!metal_pass_prepare_targets(ctx, pass, renderSize, targets))
    {
        return false;
    }
    if (metal_nanovg_begin(ctx, pass, targets.size))
    {
        python_run_pass(ctx.vg, pass.pass, targets.size);
        metal_nanovg_end(ctx);
        return true;
    }
    return false;
}
```

- [ ] **Step 4: Verify a scripted fixture**

Create or choose a scenegraph with a scripted pass and add:

```cmake
add_test(NAME vklive_render_parity_scripted_metal
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/tests/content/render_parity/scripted
        metal)
```

Run:

```sh
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -R vklive_render_parity_scripted_metal
```

Expected: scripted pass draws into its Metal target and exits cleanly.

- [ ] **Step 5: Commit**

```sh
git add include/vklive/metal/metal_nanovg.h src/metal/metal_nanovg.mm include/vklive/metal/metal_context.h src/metal/metal_device.mm src/metal/metal_pass.mm CMakeLists.txt
git commit -m "feat: support scripted passes on Metal"
```

---

### Task 8: Implement Metal ImGui Multi-Viewports

**Files:**
- Modify: `src/metal/metal_imgui.mm`
- Modify: `src/metal/imgui_impl_metal_1917.mm`
- Test: manual viewport smoke

- [ ] **Step 1: Enable viewport config flag**

In `imgui_init`, replace the validation-only branch with:

```cpp
if (viewports)
{
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
}
```

Apply the same style normalization Vulkan uses:

```cpp
ImGuiStyle& style = ImGui::GetStyle();
if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
{
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
}
```

- [ ] **Step 2: Verify renderer callbacks use SDL/Cocoa handles correctly**

Review `ImGui_ImplMetal_CreateWindow`, `ImGui_ImplMetal_RenderWindow`, and `ImGui_ImplMetal_DestroyWindow` in `src/metal/imgui_impl_metal_1917.mm`. Confirm secondary viewport creation works from the SDL platform backend on macOS.

- [ ] **Step 3: Run manual smoke**

Run:

```sh
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal
```

Manual check:

- Enable viewports in settings if the UI exposes it.
- Drag a docked window outside the main window.
- Confirm the detached window renders and can be closed.

- [ ] **Step 4: Commit**

```sh
git add src/metal/metal_imgui.mm src/metal/imgui_impl_metal_1917.mm
git commit -m "feat: enable Metal ImGui viewports"
```

---

### Task 9a: Add Metal Acceleration-Structure Foundation And Precise Diagnostics

**Files:**
- Create: `include/vklive/metal/metal_model_as.h`
- Create: `src/metal/metal_model_as.mm`
- Modify: `include/vklive/metal/metal_model.h`
- Modify: `src/metal/metal_model.mm`
- Modify: `src/metal/metal_scene.mm`
- Create: `tests/content/render_parity/metal_as/default.scenegraph`
- Create: `tests/content/render_parity/metal_as/metal_as.vert`
- Create: `tests/content/render_parity/metal_as/metal_as.frag`
- Modify: `CMakeLists.txt`

Direct `.rgen`/`.rmiss`/`.rchit` SPIR-V to native Metal ray functions is not supported by the current SPIRV-Cross MSL path. This task therefore builds the reusable acceleration-structure foundation and keeps ray passes rejected with a precise diagnostic instead of claiming false ray-tracing parity.

- [ ] **Step 1: Add Metal acceleration-structure fields**

Add to `MetalModel`:

```cpp
void* bottomLevelAccelerationStructure = nullptr;
void* topLevelAccelerationStructure = nullptr;
void* accelerationScratchBuffer = nullptr;
void* accelerationInstanceBuffer = nullptr;
bool accelerationStructuresBuilt = false;
```

- [ ] **Step 2: Build BLAS/TLAS for `buildAS` models**

Create `metal_model_build_acceleration_structures(MetalContext& ctx, MetalScene& scene, MetalModel& model)` in `src/metal/metal_model_as.mm`. Use `MTLAccelerationStructureTriangleGeometryDescriptor` with the staged vertex and index buffers, then build a BLAS and one-instance identity TLAS with `MTLAccelerationStructureCommandEncoder`.

- [ ] **Step 3: Gate support and report precise diagnostics**

Guard the AS build with `@available(macOS 11.0, *)` and `[device supportsRaytracing]`. Unsupported macOS/device combinations must not crash or block ordinary raster rendering.

Keep `PassType::RayTracing` rejected in `metal_validate_pass`, but replace the generic rejection cascade with one precise error:

```text
Metal can build acceleration structures for build_as models, but Vulkan ray shader stages (.rgen/.rmiss/.rchit) cannot be translated to Metal; native Metal ray shaders and dispatch are required.
```

- [ ] **Step 4: Verify foundation and diagnostic**

Add:

```cmake
add_test(NAME vklive_render_parity_metal_as_metal
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/tests/content/render_parity/metal_as
        metal)

add_test(NAME vklive_render_parity_ray_tracer_metal_diagnostic
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/run_tree/projects/ray_tracer
        metal
        3
        "Metal can build acceleration structures for build_as models, but Vulkan ray shader stages (.rgen/.rmiss/.rchit) cannot be translated to Metal; native Metal ray shaders and dispatch are required.")
```

Run:

```sh
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -R 'vklive_render_parity_metal_as_metal|vklive_render_parity_ray_tracer_metal_diagnostic'
```

- [ ] **Step 5: Commit**

```sh
git add include/vklive/metal/metal_model.h include/vklive/metal/metal_model_as.h src/metal/metal_model.mm src/metal/metal_model_as.mm src/metal/metal_scene.mm tests/content/render_parity/metal_as CMakeLists.txt docs/superpowers/plans/2026-05-21-metal-vulkan-parity.md
git commit -m "feat: add Metal acceleration structure foundation"
```

---

### Task 9b: Implement Native Metal Ray Shader And Dispatch Path

**Files:**
- Create: `include/vklive/metal/metal_raytrace.h`
- Create: `src/metal/metal_raytrace.mm`
- Modify: `include/vklive/metal/metal_context.h`
- Modify: `include/vklive/metal/metal_model.h`
- Modify: `include/vklive/metal/metal_pass.h`
- Modify: `src/metal/metal_pass.mm`
- Modify: `src/metal/metal_shader.mm`
- Test: `tests/render_parity_harness.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Choose the native Metal ray shader input**

Do not depend on SPIRV-Cross to translate Vulkan ray shader stages. Either add explicit `.metal` ray shader support for ray passes or define a small VkLive-native ray shader input that can generate Metal ray functions intentionally.

- [ ] **Step 2: Add ray shader compilation path**

Extend `MetalShaderStage` to include:

```cpp
RayGen,
ClosestHit,
AnyHit,
Miss,
Intersection,
Callable
```

For native Metal ray shaders, compile Metal functions into visible/intersection functions or compute kernels as required. If a project still uses Vulkan `.rgen`/`.rmiss`/`.rchit`, report a scene error naming the shader and the required rewrite path.

- [ ] **Step 3: Add Metal ray dispatch**

Create `metal_raytrace_draw(MetalContext& ctx, MetalPass& pass, const MetalPassTargets& targets)` that:

- Binds output storage target textures.
- Binds sampled surfaces.
- Binds TLAS resources.
- Dispatches ray work over `targets.size.x` by `targets.size.y`.

- [ ] **Step 4: Route `PassType::RayTracing`**

Permit `PassType::RayTracing` in `metal_validate_pass`. In `metal_pass_draw`, branch:

```cpp
if (pass.pass.passType == PassType::RayTracing)
{
    MetalPassTargets targets;
    return metal_pass_prepare_targets(ctx, pass, renderSize, targets) &&
        metal_pass_prepare_samplers(ctx, pass) &&
        metal_raytrace_draw(ctx, pass, targets);
}
```

- [ ] **Step 5: Verify ray tracer sample**

Run:

```sh
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -R vklive_render_parity_ray_tracer_metal
```

Add:

```cmake
add_test(NAME vklive_render_parity_ray_tracer_metal
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/run_tree/projects/ray_tracer
        metal)
```

Expected: ray tracer scene produces a default color output on Metal.

- [ ] **Step 6: Commit**

```sh
git add include/vklive/metal/metal_model_as.h src/metal/metal_model_as.mm include/vklive/metal/metal_raytrace.h src/metal/metal_raytrace.mm include/vklive/metal/metal_context.h include/vklive/metal/metal_model.h include/vklive/metal/metal_pass.h src/metal/metal_model.mm src/metal/metal_pass.mm src/metal/metal_shader.mm CMakeLists.txt
git commit -m "feat: add Metal ray tracing path"
```

---

### Task 10: Implement Geometry Shader Compatibility

**Files:**
- Create: `include/vklive/metal/metal_geometry_compat.h`
- Create: `src/metal/metal_geometry_compat.mm`
- Modify: `src/metal/metal_scene.mm`
- Modify: `src/metal/metal_pass.mm`
- Modify: `src/metal/metal_shader.mm`
- Test: `tests/render_parity_harness.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Classify geometry shader usage**

Add a compatibility classifier:

```cpp
enum class MetalGeometryCompatibility
{
    NotGeometry,
    Passthrough,
    ExpandLinesFromTriangles,
    Unsupported
};

MetalGeometryCompatibility metal_geometry_classify(const fs::path& shaderPath);
```

Initial supported compatibility classes:

- `Passthrough`: geometry shader emits the same triangle vertices it receives.
- `ExpandLinesFromTriangles`: geometry shader emits line-list normals/debug primitives from triangle input.

- [ ] **Step 2: Add compute or CPU expansion for supported classes**

For `Passthrough`, omit the geometry stage and use vertex+fragment stages directly.

For `ExpandLinesFromTriangles`, create an expanded Metal model buffer before drawing the pass:

```cpp
bool metal_geometry_expand_lines_from_triangles(MetalContext& ctx, MetalModel& source, MetalModel& expanded);
```

The expansion reads indexed triangles from CPU model data, creates line vertices, stages a Metal vertex/index buffer, and draws with `MTLPrimitiveTypeLine`.

- [ ] **Step 3: Route `.geom` pass setup**

Permit `.geom` in Metal scene validation only when `metal_geometry_classify(shaderPath) != Unsupported`. In `metal_pass_get_shaders`, store the geometry compatibility mode on `MetalPass`.

- [ ] **Step 4: Draw compatible geometry modes**

In `metal_pass_encode_draw`, choose primitive type:

```objc
const auto primitiveType = pass.geometryCompatibility == MetalGeometryCompatibility::ExpandLinesFromTriangles
    ? MTLPrimitiveTypeLine
    : MTLPrimitiveTypeTriangle;
```

Use expanded model buffers for line expansion passes.

- [ ] **Step 5: Verify existing geometry samples**

Run:

```sh
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -R "vklive_render_parity_(default|protoplanetary)_metal"
```

Add:

```cmake
add_test(NAME vklive_render_parity_protoplanetary_metal
    COMMAND vklive_render_parity_harness
        $<TARGET_FILE:Rezonality>
        ${CMAKE_SOURCE_DIR}/run_tree/projects/shadertoy/protoplanetary_disc
        metal)
```

Expected: supported geometry scenes run on Metal; unsupported `.geom` files produce one editor-visible diagnostic with the shader path and unsupported pattern.

- [ ] **Step 6: Commit**

```sh
git add include/vklive/metal/metal_geometry_compat.h src/metal/metal_geometry_compat.mm src/metal/metal_scene.mm src/metal/metal_pass.mm src/metal/metal_shader.mm CMakeLists.txt
git commit -m "feat: add Metal geometry shader compatibility"
```

---

## Final Verification

Run the full local verification suite:

```sh
git diff --check
cmake --build build --config Debug
ctest --test-dir build --output-on-failure
```

Run the Metal parity sample matrix:

```sh
ctest --test-dir build --output-on-failure -R "vklive_render_parity_.*_metal"
```

Run representative manual checks:

```sh
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/default
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/deferred_shading
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/pbr_robot
./build/Rezonality.app/Contents/MacOS/Rezonality --renderer metal --project run_tree/projects/ray_tracer
```

Expected final state:

- The Metal renderer starts cleanly.
- Existing Vulkan tests continue to pass.
- Metal startup-frame parity tests pass for simple, default, audio, deferred, PBR, scripted, ray tracer, and geometry samples.
- Unsupported Metal geometry patterns produce precise scene diagnostics rather than crashes or silent rendering failure.

## Execution Notes

- Commit the existing startup-crash fix before beginning Task 1.
- Keep one commit per task.
- Do not edit vendored libraries unless a task demonstrates that the missing behavior cannot be implemented in project-owned code.
- When a task touches both Vulkan and Metal behavior, run both the existing CTest suite and the targeted Metal parity CTest before committing.
