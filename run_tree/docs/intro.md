# Welcome to Rezonality!

## Navigating
CTRL+H/L - Swap Tabs
CTRL+ENTER - Evaluate scene
CTRL+1/2 - Switch between notepad and Vim modes
CTRL+P - Fuzzy search in project (or use the Asset menu)
Cursor-over a red error to see the contents
 
## SceneGraph
- The scene graph file has a simple format.  First you declare passes, then geometries within them.
- Use !pass to disable a pass from being drawn.
- Inside the pass you can request a clear of the render target, supply shaders and shapes to draw.
      
## Troubleshooting
- If you get the tool into a broken state, try removing the imgui.ini file to reset the layout, or try deleting the configuration from ApData/Local/VkLive/settings.  The contents of the setting file should be fairly obvious if you want to tweak it.
- If anything doesn't compile, the scene will keep rendering from the last known good state if possible.
- If you make a project that breaks the tool, please zip up the folder and send it as a bug report, so we can fix it!