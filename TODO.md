TODO
----

## General
- Ping-Pong option (10)
- Create UBO for each frame?
- Icon (1)
- Restart menu option to restart clocks, frame count, and do default clear once
- Blend operations/Alpha, and other render state
- Cleanup scene dynamic state setup

- Fix Fixed Parameters:
    iMouse (x,y screen)
    iDate (year, month, day, seconds since EPOCH)
    fragOffset (audio)

- Check low DPI (5)

## Project Support
- [Need to think about this stuff some more]
- Auto create missing shaders (1)
    - Make a function which walks through the components of a working project/scene and rebuilds what is necessary
    - This function will also update visible edited shaders and make new ones when the user compiles the scenegraph file, using default templates
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
## General
- On a release build, after compiling shaders, very occasional crash

## Zep
- Flicker when too small
- Tabs can be hard to delete/change to spaces
- Word wrapping on words in the help docs would be useful.
- Normal mode; Shift+ doesn't work for swapping tabs
- Normal mode; insertion of <> when shift/arrow, etc.
- Support clang format for easy shader formatting; even after micro edits?
- Convert tabs to spaces / vice versa
- Paste should probably normalize tabs to spaces

Fridge
------
- Build acceleration structure from a model (10)
- Allow ray trace operations (20)
- Default shapes, sphere, torus, etc. (5)
- Variable panel for user tweakables (20)
- User layouts for imgui windows (10)
- Support Tesselation shaders (5)
- Cubemap/Volume (10)
 
# Zep
- Highlight multiple lines and tab
- Wordwrapping on word boundaries (10)

Freezer
-------
# Mac
- Make a native menu for more consistency?
- Metal renderer
# PC
- DX12
