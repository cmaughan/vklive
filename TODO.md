TODO
----

## Project Support
- Auto create missing shaders (1)
    - Make a function which walks through the components of a working project/scene and rebuilds what is necessary
    - This function will also update visible edited shaders and make new ones when the user compiles the scenegraph file
- Handle loading from an empty folder using above

## General
- Create targets in scene file, read targets in passes.  Ping-Pong option (10)
- Supply texture targets and textures to scene files (10)
- Supply ShaderToy-like variables to shaders (const uniform structure for now) (2)
- Icon (1)
- Better name? (1)
- Test various error scenarios; especially scene build failures (ongoing)
- Detect device issues, handle fallbacks, etc.
- Check low DPI

## Mac
- Get correct documents folder (1)
- Copy run_tree to bundle (1)

## PC
- Copy run_tree to installer location (1)

## Linux
- Test/fix the working build (not tried yet) (10)

## Samples
- Write cleaner shader samples
- Fix existing/test samples for syntax changes
-   target write/read
-   texture/shapes
-   geometry shader

Bugs
----
## Zep
- Flicker when too small
- Tabs can be hard to delete/change to spaces
- Word wrapping on words in the help docs would be useful.

Fridge
------
- Build acceleration structure from a model (10)
- Allow ray trace operations (20)
- Default shapes, sphere, torus, etc. (5)
- Audio input as an FFT texture (10)
- Variable panel for user tweakables (20)
- User layouts for imgui windows (10)
- Support Tesselation shaders (5)
- Cubemap/Volume (5)
- Noise map (5)
# Zep
- Highlight multiple lines and tab
- Convert tabs to spaces / vice versa

Freezer
-------
# Mac
- Make a native menu for more consistency?
- Metal renderer
# PC
- DX12

Done
----
- Config file: last file loaded, etc.
- Load template projects, copy to temp
- Auto make a temp project on startup
- On exit, ask for saving of temporary projects
- Display errors in editor
- Switch to other projects
- Save project as
- Scene file, simple FSQ or model passes for now
- Save/Restore main window size & position, and maximize state
- Option to draw on background instead of in a window
- Copy paste plugin for Zep
- Menu Switch notepad/vim mode : CTRL + 1/CTRL + 2 (config option)
- Menu to reload shader list 
- A popup documentation window with the shortcuts/tips/user info

# Project
- Only copy files that are part of it
- Don't copy to non-empty folder (query)
- Load a project.toml to find the scenegraph file
- Default imgui.cfg for better window layout, or programatically

## DevOps
- Github build action (5)


