# UITest

Testing out some experimental UI control type code.<br>
Currently renders using DirectDraw, but can literally be rendered with anything, just needs to be implemented in the draw function.

The only thing that I'm not happy with is during hit test check when dragging the mouse, I think I need a "focus latch" of some kind.<br>
Probably separate functions for mouse down and move, but it works for now.