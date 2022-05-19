# Welcome to Rezonality!
Welcome to "Rez".  Here are a few quick tips to get you started.  If you read nothing else, make sure you read the Navigating section.
(Close this tab using File..Close, or :clo in Vim mode)

## Navigating
CTRL+H/L - Swap Between Tabs (*very* useful).
CTRL+ENTER - Evaluate scene; try tweaking a shader file and see what happens.
CTRL+1/2 - Switch between notepad and Vim modes.
CTRL+P - Fuzzy search in project if on a project file (or use the Asset menu).
Cursor-over a red error to see the reason for it.

## Projects
Rezonality projects are just folders.  A project has a .scenegraph file, and usually a project.toml which points to it.  Saving a project involves copying all its files to a new directory (File->Save Project As...).  Open a project by opening a folder.  The easiest way to start a new one is to use the File->New From Template option.

## SceneGraph
The scene graph file has a simple format - first you declare passes, then geometries within them. 
See the default project for how it works.  Inside the pass you can request a clear of the render target, 
supply shaders and shapes to draw. 
Use !pass to disable a pass from being drawn; this is useful because commenting out things is a little tedious currently.
      
## Troubleshooting
- If you get the tool into a broken state, try removing the imgui.ini file to reset the layout, 
or try deleting the configuration from AppData/Local/VkLive/settings.  
The contents of the setting file should be fairly obvious if you want to tweak it.

- If anything doesn't compile, the scene will keep rendering from the last known good state if possible; 
if that isn't possible you might get a blank render window or no render window at all.

- If you make a project that breaks the tool, please zip up the folder and send it as a bug report, so we can fix it!

- If you make something pretty, send it to the Screenshots discussion on github.

- File bugs/reports for fixes & feature requests.

- Contribute! :)
