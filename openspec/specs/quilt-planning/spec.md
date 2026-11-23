# Quilt planning

## Purpose

Help a home quilter explore traditional and modern layouts, arrange individual fabric pieces, and take accurate cutting instructions to the sewing table.

## Requirements

### Requirement: Arrange a quilt top

The planner SHALL offer editable blocks made of individual fabric pieces, five starter fabrics and up to 26 labeled fabrics, independent row and column counts from 3 to 9, and a finished base block side from 5 to 20.32 centimetres. It SHALL allow square patches as well as triangles, rectangular strips, diamonds and other straight-edged pieces. Painting, resizing and applying a pattern SHALL work with pointer and keyboard input.

#### Scenario: Recolor one patch

- **WHEN** the quilter selects fabric B and paints a square patch
- **THEN** only that square changes to B and the cutting counts update

#### Scenario: Recolor one piece

- **WHEN** the quilter selects fabric B and paints a triangle in a block
- **THEN** only that triangle changes to B and the cutting counts update

#### Scenario: Revise an experiment

- **WHEN** the quilter shrinks the grid or applies a pattern and chooses Undo
- **THEN** the previous dimensions, geometry and fabrics return
- **AND** Redo restores the change

#### Scenario: Grow a quilt

- **WHEN** a quilter adds a row to a block quilt
- **THEN** the previous rows retain their pieces and the new row starts with fabric A squares

### Requirement: Take a cutting sheet to the sewing table

The planner SHALL provide finished top dimensions, piece quantities by fabric, placement guidance and cut dimensions. Joined pieces SHALL include a quarter-inch seam allowance on every stitched edge. Raw-edge appliqué SHALL be cut to the finished outline and identified separately. Block layouts SHALL have a row-by-row placement map. The sheet SHALL be available as plain text.

#### Scenario: Cut a five-by-five topper

- **WHEN** the quilter opens the sheet for five rows and five columns of four-inch finished squares
- **THEN** the top measures 20 by 20 inches and the quantities total 25 squares
- **AND** each square is cut at 4.5 by 4.5 inches

#### Scenario: Cut a pieced block

- **WHEN** the quilter exports the cutting sheet for a design with shaped pieces
- **THEN** each used template has quantities by fabric and dimensions including its seam allowance
- **AND** the placement map identifies the templates, fabrics and block turns

### Requirement: Resume a local plan

The planner SHALL persist completed edits on the current device, validate saved data before loading it, and show a visible status if storage fails. It SHALL load version 1 square plans, version 2 patterned plans, version 3 plans with mixed patterns, custom colors and physical block dimensions, and version 4 plans with extra fabrics and whole-quilt designs.

#### Scenario: Return to yesterday's quilt

- **WHEN** the quilter closes and reopens the application after editing a piece of a patterned quilt
- **THEN** the dimensions, geometry, pattern name and fabrics match the saved quilt

#### Scenario: Reopen a square plan

- **WHEN** the quilter opens a version 1 plan
- **THEN** the planner restores the square layout and the original fabrics

#### Scenario: Encounter an unreadable plan

- **WHEN** stored data has an unsupported version or an invalid piece
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

### Requirement: Browse traditional and modern patterns

The planner SHALL offer at least 288 distinct layouts from at least 36 named base designs with eight quilt settings. The library SHALL filter traditional and modern designs and preview a selected design and setting at the current quilt dimensions before applying it.

#### Scenario: Try Flying Geese in a border

- **WHEN** the quilter picks the traditional Flying Geese design and the Border setting
- **THEN** the preview places geese at the edges and plain fabric blocks in the center
- **AND** the active quilt changes only when she applies the pattern

#### Scenario: Compare two modern quilt settings

- **WHEN** the quilter picks a modern design and switches from Rows to Alternating
- **THEN** the arrangement of the patterned and plain blocks changes
- **AND** the preview keeps the same quilt dimensions

### Requirement: Compose with blocks and staggered seams

The planner SHALL let a quilter place different named blocks on one quilt, rotate them before placing, and use regular, horizontally staggered or vertically staggered seams. The previews, hit testing, quantities and cutting templates SHALL follow the seam geometry.

#### Scenario: Combine two traditional blocks

- **WHEN** the quilter places a Flying Geese block and a Log Cabin block on different positions
- **THEN** both motifs appear on the same quilt and their pieces can be edited separately
- **AND** Undo and saved plans retain the correct block types and turns

#### Scenario: Offset alternating brick rows

- **WHEN** the quilter changes the quilt to horizontally staggered seams
- **THEN** alternate rows have offset joints and different edge-block widths
- **AND** the cutting sheet uses the actual dimensions of those pieces

### Requirement: Use inches or centimetres

The planner SHALL accept the finished block edge in inches or centimetres and SHALL present cutting measurements in the selected unit. Changing between the units SHALL preserve the quilt's physical dimensions.

#### Scenario: Measure a ten-centimetre block

- **WHEN** the quilter enters 10 cm for a block edge
- **THEN** five blocks measure 50 cm across
- **AND** changing to inches does not round the stored size or change the seam allowance

### Requirement: Choose a fabric palette

The planner SHALL offer approximately 100 named palettes and let a quilter assign any sRGB color to each fabric in a basket of up to 26 slots. Color changes SHALL be undoable and saved with the quilt.

#### Scenario: Make a palette your own

- **WHEN** the quilter applies a palette and enters a custom hex color for fabric C
- **THEN** every piece using C changes to the chosen color
- **AND** reopening the quilt restores the palette and the custom color

#### Scenario: Add a teal scrap

- **WHEN** the quilter adds fabric F, enters a teal RGB value and paints a piece with it
- **THEN** the map, fabric quantities and reopened quilt identify that piece as F
- **AND** choosing a named five-color combination retains F and its pieces

### Requirement: Export a PDF cutting sheet

The planner SHALL offer a PDF that includes the colored layout diagram and the cutting sheet in the current language and units.

#### Scenario: Print a mixed quilt in Polish

- **WHEN** the quilter downloads the PDF with Polish and centimetres selected
- **THEN** the PDF contains the quilt map and Polish instructions, including dimensions in centimetres
- **AND** Polish letters remain legible in a standard PDF viewer

### Requirement: Piece across the whole quilt

The planner SHALL preview and apply hexagon, tumbling-block and clamshell tessellations with individual editable pieces, joined edges and fitted perimeter pieces. It SHALL support raw-edge botanical, koi and woven appliqué on wholecloth or an existing pieced top. Pointer selection SHALL use the visible piece at an overlap. A ribbon that alternates over and under SHALL remain a single fabric piece.

#### Scenario: Piece a curved clamshell top

- **WHEN** a quilter fills a top with clamshells and paints one shell
- **THEN** the curved edges nest with the neighboring rows and that entire shell takes the new fabric
- **AND** the cutting sheet counts the shell once

#### Scenario: Put a vine on a mixed quilt

- **WHEN** the quilter adds Flowering Vine over two different block motifs
- **THEN** the stems run across block seams and the leaves and flowers can be recolored independently
- **AND** removing the appliqué reveals the same block layout

#### Scenario: Trace an actual-size piece

- **WHEN** the quilter exports a PDF of a quilt-wide design
- **THEN** the map and material list include the actual foundation and appliqué pieces
- **AND** labeled A4 tiles provide full-size stitching and cutting outlines for piecing and raw-edge cutting outlines for appliqué, with a scale check
