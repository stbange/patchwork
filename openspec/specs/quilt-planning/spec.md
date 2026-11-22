# Quilt planning

## Purpose

Help a home quilter arrange square fabric patches, resume her design and take accurate cutting instructions to the sewing table.

## Requirements

### Requirement: Arrange a quilt top

The planner SHALL offer an editable square-patch design with five labeled fabrics, independent row and column counts from 3 to 9, and a finished patch side from 2 to 8 inches. Painting and resizing SHALL work with pointer and keyboard input.

#### Scenario: Recolor one patch

- **WHEN** the quilter selects fabric B and paints a patch
- **THEN** only that patch changes to B and the cutting counts update

#### Scenario: Revise an experiment

- **WHEN** the quilter shrinks the grid and chooses Undo
- **THEN** the previous dimensions and all cropped patches return
- **AND** Redo restores the smaller layout

#### Scenario: Grow a quilt

- **WHEN** a quilter adds a row
- **THEN** the previous rows retain their fabrics and the new row starts in fabric A

### Requirement: Take a cutting sheet to the sewing table

The planner SHALL provide finished top dimensions, counts per fabric, a row-by-row placement map and cut dimensions that include a quarter-inch seam on every edge. The sheet SHALL be available as plain text.

#### Scenario: Cut a five-by-five topper

- **WHEN** the quilter opens the sheet for five rows and five columns of four-inch finished squares
- **THEN** the top measures 20 by 20 inches and the quantities total 25 squares
- **AND** each square is cut at 4.5 by 4.5 inches

### Requirement: Resume a local plan

The planner SHALL persist completed edits on the current device, validate saved data before loading it, and show a visible status if storage fails.

#### Scenario: Return to yesterday's quilt

- **WHEN** the quilter closes and reopens the application after editing a patch
- **THEN** the dimensions and fabrics match the saved quilt

#### Scenario: Encounter an unreadable plan

- **WHEN** stored data has an unsupported version or an invalid patch
- **THEN** the application reports the problem and preserves the original data

### Requirement: Verify the supported applications

The project SHALL build for Windows, Linux and Emscripten, enforce at least 95% unit-test line coverage of first-party C compiled for the native test build, and test the built desktop and browser applications end to end.

#### Scenario: Catch a broken user workflow

- **WHEN** the desktop application is launched or the WebAssembly application is loaded from HTTP in a browser
- **THEN** automated input exercises editing, the cutting sheet and reopening the saved layout

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
