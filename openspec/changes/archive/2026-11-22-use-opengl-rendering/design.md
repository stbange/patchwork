## Decisions

Use SDL's renderer selection at startup. On desktop, try OpenGL first and OpenGL ES second. Emscripten maps SDL's OpenGL ES 2 renderer to a WebGL context on the planner canvas. SDL handles textures, geometry, fonts and window resizing through the chosen renderer. Use its software backend as the compatibility fallback. Keep renderer selection at default hint priority so a user or headless test can request a specific driver. [SDL renderer selection](https://wiki.libsdl.org/SDL3/SDL_CreateRenderer) · [Emscripten OpenGL support](https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html)

Use 64-by-64 fabric tiles so WebGL 1 can repeat each motif across large patches.

For example, repainting the center of Garden Path should visibly turn honey into rose. Undo should bring back the honey fabric, and the cutting sheet should still count one honey patch. Exercise that edit on the actual graphics backend. The optional 3D table preview in the original concept can build on the same quilt model later.
