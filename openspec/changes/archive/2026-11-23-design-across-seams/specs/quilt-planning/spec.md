## MODIFIED Requirements

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

## ADDED Requirements

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
