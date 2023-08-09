TODO
----

# Backlog
---------
## Priority
- Auto Indent.  Adding a return after a { should indent for convenience.  Return within that same group should indent to the previous line begin level.  Experiment with VC and try to copy the behavior.

## General
- Cleanup the binding code, separate it out a bit more.  Bindings are typically built dynamically.

- Supply model textures in a descriptor, do a textured model sample

- Blend operations/Alpha, and other render state

- Option to one-off clear a surface, and with restart menu
    - Restart menu option should do default clear once

- Fix Parameters:
    iMouse (x,y screen)
    iDate (year, month, day, seconds since EPOCH)
    fragOffset (audio)

- App Icon

- Auto create missing shaders
    - Make a function which walks through the components of a working project/scene and rebuilds what is necessary?
    - This function will also update visible edited shaders and make new ones when the user compiles the scenegraph file, using default templates?
    - Handle loading from an empty folder using this approach?

- Load/Save a project from a zip file?

- Support Volume and Cubemap textures

- Full screen mode
-
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


# Bugs
------
## General
- Check low DPI; need a machine to do this on

- Left over objects on shut down ; need to reconfirm this.

- Tabs need to scroll horizontally in the correct way

- Visible lines calculation seems incorrect.

## Zep
- Word wrapping on words in the help docs would be useful.

- Normal mode; Shift+ doesn't work for swapping tabs


# Fridge
--------
- User layouts for imgui windows (10)
- Build acceleration structure from a model (10)
- Allow ray trace operations (20)
- Default shapes, sphere, torus, etc. (5)
- Variable panel for user tweakables (20)
- Cubemap/Volume (10)

# Zep
- Highlight multiple lines and tab for faster block indenting (10)
- Wordwrapping on word boundaries (20)


# Freezer
---------
# Rendering
- Support Tesselation shaders (5)

# Mac
- Make a native menu for more consistency?
- Metal renderer

# PC
- DX12
