TODO
----

## General
- Cleanup the binding code, separate it out a bit more.  Bindings are typically built dynamically.

- Supply model textures in a descriptor

- Blend operations/Alpha, and other render state

- Option to one-off clear a surface, and with restart menu
    - Restart menu option should do default clear once

- Fix Parameters:
    iMouse (x,y screen)
    iDate (year, month, day, seconds since EPOCH)
    fragOffset (audio)

- App Icon

- Auto create missing shaders
    - Make a function which walks through the components of a working project/scene and rebuilds what is necessary
    - This function will also update visible edited shaders and make new ones when the user compiles the scenegraph file, using default templates
    - Handle loading from an empty folder using this approach

- Load/Save a project from a zip file?

- Support Volume and Cubemap textures

- Check low DPI; need a machine to do this on

## Mac
- Get correct documents folder; I don't think this is correctly located yet

## Linux
- Test on linux machine

## Samples
- Write cleaner shader samples
- Fix existing/test samples for syntax changes
-   target write/read
-   texture/shapes
-   geometry shader
-   ping pong example

Bugs
----
## General
- On a release build, after compiling shaders, very occasional crash
- Left over objects on shut down 

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
- Cubemap/Volume (10)
 
# Zep
- Highlight multiple lines and tab for faster block indenting (10)
- Wordwrapping on word boundaries (20)

Freezer
-------
# Rendering
- Support Tesselation shaders (5)

# Mac
- Make a native menu for more consistency?
- Metal renderer

# PC
- DX12
