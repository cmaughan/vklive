TODO
----

## General
- Create targets in scene file, read targets in passes.  Ping-Pong option (10)
- Supply texture targets and textures to scene files (10)
- Icon (1)
- Test various error scenarios; especially scene build failures (ongoing)
- Detect device issues, handle fallbacks, etc.
- Check low DPI (5)

- Fix Fixed Parameters:
    iMouse (x,y screen)
    iDate (year, month, day, seconds since EPOCH)
    fragOffset (audio)
    iChannel (input channels)

## Project Support
- [Need to think about this stuff some more]
- Auto create missing shaders (1)
    - Make a function which walks through the components of a working project/scene and rebuilds what is necessary
    - This function will also update visible edited shaders and make new ones when the user compiles the scenegraph file
- Handle loading from an empty folder using above

## Mac
- Get correct documents folder (1)

## Linux
- Test on linux machine

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
- Wordwrapping on word boundaries (10)

Freezer
-------
# Mac
- Make a native menu for more consistency?
- Metal renderer
# PC
- DX12
