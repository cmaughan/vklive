# PBR glTF Scene Format Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend VkLive scenes so a template can load a glTF model, sample an HDR environment, render the environment around the model, and keep failures as scene errors instead of application crashes.

**Architecture:** Add small scene-format aliases for named model and environment assets while preserving existing `geometry { path: ... }` and `surface { path: ... }` behavior. Treat `environment` as a `Surface` for the first slice so it works with the current descriptor/reflection system, then leave room for later cubemap/IBL precomputation. Add HDR decoding to the existing `VulkanSurface` loader and ship a self-contained `run_tree/projects/pbr_robot` template.

**Tech Stack:** C++20, mpc scene parser, Assimp glTF loader, stb_image HDR loading, Vulkan sampled images, GLSL 450 shaders, CMake via `config.bat` and `build.bat`.

---

## File Structure

- Modify `include/vklive/scene.h`: add named model asset storage and geometry model-reference fields.
- Modify `src/scene.cpp`: extend grammar with `model:` and `environment:`, parse top-level declarations, and resolve `geometry { model: ... }`.
- Modify `src/vulkan/vulkan_surface.cpp`: add HDR decode path and safer RGBA8 LDR decode path.
- Create `run_tree/projects/pbr_robot/project.toml`: template project settings.
- Create `run_tree/projects/pbr_robot/default.scenegraph`: sample scene using a named model and HDR environment.
- Create `run_tree/projects/pbr_robot/skybox.vert`: fullscreen sky pass vertex shader.
- Create `run_tree/projects/pbr_robot/skybox.frag`: equirectangular HDR environment shader.
- Create `run_tree/projects/pbr_robot/pbr.vert`: model vertex shader.
- Create `run_tree/projects/pbr_robot/pbr.frag`: lightweight physically based shader using the HDR environment.
- Copy `../games/rts/assets/environments/farm_field_puresky_1k.hdr` to `run_tree/projects/pbr_robot/textures/environment/farm_field_puresky_1k.hdr`.
- Copy `../games/rts/assets/models/robot` to `run_tree/projects/pbr_robot/models/robot`.
- Create or extend tests under `tests/` for plan-level regression checks that can run without launching the GUI.

---

### Task 1: Add Plan and Regression Fixtures

**Files:**
- Create: `plans/2026-05-19-pbr-gltf-scene-format.md`
- Create: `tests/test_pbr_template.py`

- [ ] **Step 1: Write the failing tests**

Create a Python unittest that checks the new template files and scene syntax exist before implementation. The test should fail initially because `run_tree/projects/pbr_robot` does not exist.

```python
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "run_tree" / "projects" / "pbr_robot"


class PbrTemplateTests(unittest.TestCase):
    def test_template_contains_named_model_environment_and_passes(self):
        scene = TEMPLATE / "default.scenegraph"
        text = scene.read_text(encoding="utf-8")

        self.assertIn("environment: studio_sky", text)
        self.assertIn("model: robot", text)
        self.assertIn("model: robot", text)
        self.assertIn("samplers: studio_sky", text)
        self.assertIn("path: models/robot/scene.gltf", text)
        self.assertIn("path: textures/environment/farm_field_puresky_1k.hdr", text)

    def test_template_assets_are_self_contained(self):
        self.assertTrue((TEMPLATE / "models" / "robot" / "scene.gltf").is_file())
        self.assertTrue((TEMPLATE / "models" / "robot" / "scene.bin").is_file())
        self.assertTrue((TEMPLATE / "textures" / "environment" / "farm_field_puresky_1k.hdr").is_file())

    def test_template_shaders_bind_expected_environment_sampler(self):
        skybox = (TEMPLATE / "skybox.frag").read_text(encoding="utf-8")
        pbr = (TEMPLATE / "pbr.frag").read_text(encoding="utf-8")

        self.assertIn("uniform sampler2D studio_sky", skybox)
        self.assertIn("uniform sampler2D studio_sky", pbr)
        self.assertIn("distributionGGX", pbr)
```

- [ ] **Step 2: Run the test and verify it fails**

Run: `python -m unittest tests.test_pbr_template`

Expected: `FileNotFoundError` or assertion failure because the template has not been created.

---

### Task 2: Extend Scene Format

**Files:**
- Modify: `include/vklive/scene.h`
- Modify: `src/scene.cpp`

- [ ] **Step 1: Add named model data**

Add a small model asset structure:

```cpp
struct ModelAsset
{
    std::string name;
    fs::path path;
    glm::vec3 loadScale = glm::vec3(1.0f);
    bool buildAS = false;
};
```

Add to `Geometry`:

```cpp
std::string modelName;
```

Add to `Scene`:

```cpp
std::map<std::string, std::shared_ptr<ModelAsset>> modelAssets;
```

- [ ] **Step 2: Extend parser tokens**

Add parser tags for `model`, `model_ref`, and `environment`. `environment` should parse like `surface` and be stored in `Scene::surfaces`, with the environment name available as a normal sampler.

The grammar shape should be:

```txt
model_ref        : "model" ':' <ident> ;
model            : "model" ':' <ident> '{' (<comment> | <path> | <scale> | <build_as>)* '}';
environment      : "environment" ':' <ident> '{' (<comment> | <path> | <format> | <scale> | <size>)* '}';
geometry         : "geometry" ':' <ident> '{' (<path> | <model_ref> | <scale> | <build_as> | <ray_group_general> | <ray_group_triangles> | <ray_group_procedural> | <vs> | <fs> | <gs> | <comment>)* '}';
scenegraph       : /^/ (<comment> | <surface> | <environment> | <model> | <camera>)* (<comment> | <pass> )* <post_2d>? /$/ ;
```

- [ ] **Step 3: Parse top-level models**

For each top-level `model`, resolve `path` through `scene_find_asset(..., AssetType::Model)`. If missing, add an error and skip the declaration. Apply optional `scale` and `build_as`.

- [ ] **Step 4: Parse environments as surfaces**

For each `environment`, create a `Surface` with the environment name. Parse `path`, `format`, `scale`, and `size` exactly like `surface`. If no format is supplied, default to `r32g32b32a32_sfloat` so `.hdr` files get a float Vulkan image.

- [ ] **Step 5: Resolve geometry model references**

Inside pass geometry, allow either `path:` or `model:`. If both are present, report an error and prefer `path:` for backward compatibility. If `model:` is unknown, report an error and skip that geometry. Copy the referenced `ModelAsset` path, scale, and `buildAS` into the `Geometry`.

- [ ] **Step 6: Verify build**

Run: `.\build.bat`

Expected: compiler succeeds; existing scenes still parse because old `geometry { path: ... }` syntax remains valid.

---

### Task 3: Add HDR and Safer Texture Loading

**Files:**
- Modify: `src/vulkan/vulkan_surface.cpp`

- [ ] **Step 1: Add HDR load branch**

In `surface_create_from_memory`, before the normal `stbi_load_from_memory` path, check:

```cpp
if (stbi_is_hdr_from_memory(reinterpret_cast<const stbi_uc*>(pData), static_cast<int>(data_size)))
```

Load with `stbi_loadf_from_memory(..., STBI_rgb_alpha)`, create a `vk::Format::eR32G32B32A32Sfloat` image unless the scene explicitly requested `eR16G16B16A16Sfloat`, and upload `x * y * 4 * sizeof(float)` bytes.

- [ ] **Step 2: Normalize LDR image loading**

Use `stbi_load_from_memory(..., STBI_rgb_alpha)` for non-HDR files and upload `x * y * 4` bytes. If stb returns `nullptr` or invalid dimensions, mark allocation failed and return `false`.

- [ ] **Step 3: Free stb memory**

Call `stbi_image_free` after successful staging for both HDR and LDR paths.

- [ ] **Step 4: Verify build**

Run: `.\build.bat`

Expected: compiler succeeds.

---

### Task 4: Add PBR Robot Template

**Files:**
- Create: `run_tree/projects/pbr_robot/project.toml`
- Create: `run_tree/projects/pbr_robot/default.scenegraph`
- Create: `run_tree/projects/pbr_robot/skybox.vert`
- Create: `run_tree/projects/pbr_robot/skybox.frag`
- Create: `run_tree/projects/pbr_robot/pbr.vert`
- Create: `run_tree/projects/pbr_robot/pbr.frag`
- Copy assets into `run_tree/projects/pbr_robot/models/robot`
- Copy HDR into `run_tree/projects/pbr_robot/textures/environment`

- [ ] **Step 1: Copy assets**

Copy the robot model directory and HDR file from `../games/rts/assets` into the template so it is self-contained and compatible with the current scene path grammar.

- [ ] **Step 2: Add scenegraph**

Create a scene with:

```txt
environment: studio_sky {
    path: textures/environment/farm_field_puresky_1k.hdr
    format: rgba32f
}

model: robot {
    path: models/robot/scene.gltf
    scale: (0.65, 0.65, 0.65)
}
```

Add a sky pass that renders `screen_rect` with `skybox.vert`/`skybox.frag`, then a model pass that references `model: robot`, uses `pbr.vert`/`pbr.frag`, and samples `studio_sky`.

- [ ] **Step 3: Add shaders**

The sky shader should sample the equirectangular HDR texture from view direction. The model fragment shader should implement a compact GGX/Smith/Schlick PBR approximation with environment lighting and tone mapping.

- [ ] **Step 4: Run template tests**

Run: `python -m unittest tests.test_pbr_template`

Expected: tests pass.

---

### Task 5: Final Verification

**Files:**
- All changed files.

- [ ] **Step 1: Configure**

Run: `.\config.bat`

Expected: exit code `0`.

- [ ] **Step 2: Build**

Run: `.\build.bat`

Expected: exit code `0`.

- [ ] **Step 3: Run Python tests**

Run: `python -m unittest tests.test_do tests.test_pbr_template`

Expected: all tests pass.

- [ ] **Step 4: Inspect git diff**

Run: `git diff --stat`

Expected: changes are limited to the plan, scene/parser/HDR implementation, tests, and the new template assets.

---

## Later Follow-Ups

- Extract glTF external material texture loading into `ModelMaterial` and upload per-material descriptors.
- Draw each `ModelPart` with its material index instead of a single model-wide draw call.
- Add tangent-space normal mapping by extending the default vertex layout or adding per-template vertex layouts.
- Generate cubemap, irradiance, prefiltered environment, and BRDF LUT textures from the HDR environment.
- Keep the previous valid GPU resources alive during hot reload if a replacement model, texture, or shader fails to compile.
