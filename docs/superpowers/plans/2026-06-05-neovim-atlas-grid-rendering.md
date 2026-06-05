# Neovim Atlas/Grid Rendering Port

## Goal

Replace the first-pass ImGui text renderer for the embedded Neovim editor with Draxul's mature font atlas and grid-cell rendering path, while keeping this a VkLive-first integration.

## Scope

- Copy the required Draxul font/grid concepts into `libs/vklive_nvim`; do not add Draxul as a submodule.
- Use FreeType/HarfBuzz shaping and a Nerd Font atlas so icon glyphs render correctly.
- Keep the first implementation inside the existing Metal/Vulkan ImGui wrapper by drawing atlas quads with ImGui draw lists.
- Add an RGBA atlas upload path to the existing backend texture abstraction so color glyph/atlas data is preserved.
- Keep Zep and Neovim switchable through the existing editor mode path.

## Out Of Scope

- Native Vulkan/Metal grid passes.
- Live-coding layer integration beyond opening shader files as Neovim tabs.
- Draxul terminal-host features and extra host modes.

## Implementation Tasks

1. Add the Draxul-derived font atlas types and `TextService` to `libs/vklive_nvim`, renamed into the `vklive_nvim` namespace.
2. Add FreeType/HarfBuzz dependencies and link them only into `vklive_nvim`.
3. Copy the JetBrains Mono Nerd Font runtime assets into `run_tree/fonts`.
4. Add a small atlas grid builder that converts `RenderModel` cells into atlas-backed cell quads.
5. Extend the backend font texture interface with RGBA create/update methods and implement them for Vulkan and Metal.
6. Replace `NvimImGuiRenderer::AddText` drawing with background rectangles plus atlas `AddImage` glyph quads.
7. Add focused tests for text service atlas generation and atlas-grid cell conversion.
8. Verify with Release build, because that is the locally working configuration for this branch.
