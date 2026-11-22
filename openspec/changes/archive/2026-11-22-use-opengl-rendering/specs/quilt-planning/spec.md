## ADDED Requirements

### Requirement: Draw the planner with OpenGL

The planner SHALL prefer OpenGL for the design wall and cutting sheet on Windows and Linux and WebGL in the Emscripten build. It SHALL offer software rendering when a compatible GL context cannot be created.

#### Scenario: Edit a quilt on an OpenGL desktop

- **WHEN** the quilter opens Patchwork with a compatible desktop OpenGL driver and default settings
- **THEN** SDL selects the OpenGL renderer and the quilter can edit the design and take the cutting sheet

#### Scenario: Try a fabric in a WebGL browser

- **WHEN** the quilter replaces the honey center with rose on a WebGL-capable browser
- **THEN** the quilt is drawn on a WebGL canvas and the center changes visibly
- **AND** Undo restores the honey patch

#### Scenario: Open a quilt without GL

- **WHEN** GL context creation is unavailable and the platform provides software rendering
- **THEN** the quilter can edit a patch and take the cutting sheet using software rendering
