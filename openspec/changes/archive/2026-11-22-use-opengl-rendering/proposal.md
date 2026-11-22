## Why

The requested graphics technology is OpenGL. Explicit renderer selection makes the same C drawing code exercise OpenGL on desktops and WebGL in browsers.

## What Changes

- Prefer desktop OpenGL on Windows and Linux and WebGL through SDL's OpenGL ES renderer in Emscripten.
- Keep a software fallback for devices without a usable GL driver.
- Check the active backend while exercising an edit, undo and the cutting sheet.

## Capabilities

### Modified Capabilities

- `quilt-planning` gains a rendering requirement for the design wall and cutting sheet.

## Impact

The change affects SDL startup, the platform tests and the build instructions.
