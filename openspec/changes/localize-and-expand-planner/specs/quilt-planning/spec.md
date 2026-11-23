## ADDED Requirements

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

The planner SHALL offer approximately 100 named palettes and let a quilter assign any sRGB color to any of the five fabric slots. Color changes SHALL be undoable and saved with the quilt.

#### Scenario: Make a palette your own

- **WHEN** the quilter applies a palette and enters a custom hex color for fabric C
- **THEN** every piece using C changes to the chosen color
- **AND** reopening the quilt restores the palette and the custom color

### Requirement: Export a PDF cutting sheet

The planner SHALL offer a PDF that includes the colored layout diagram and the cutting sheet in the current language and units.

#### Scenario: Print a mixed quilt in Polish

- **WHEN** the quilter downloads the PDF with Polish and centimetres selected
- **THEN** the PDF contains the quilt map and Polish instructions, including dimensions in centimetres
- **AND** Polish letters remain legible in a standard PDF viewer

## MODIFIED Requirements

### Requirement: Arrange a quilt top

The planner SHALL offer editable blocks made of individual fabric pieces, five labeled fabrics, independent row and column counts from 3 to 9, and a finished base block side from 5 to 20.32 centimetres. It SHALL allow square patches as well as triangles, rectangular strips, diamonds and other straight-edged pieces. Painting, resizing and applying a pattern SHALL work with pointer and keyboard input.

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

- **WHEN** a quilter adds a row
- **THEN** the previous rows retain their pieces and the new row starts with fabric A squares

### Requirement: Resume a local plan

The planner SHALL persist completed edits on the current device, validate saved data before loading it, and show a visible status if storage fails. It SHALL load version 1 square plans, version 2 patterned plans and version 3 plans with mixed patterns, custom colors and physical block dimensions.

#### Scenario: Return to yesterday's quilt

- **WHEN** the quilter closes and reopens the application after editing a piece of a patterned quilt
- **THEN** the dimensions, geometry, pattern name and fabrics match the saved quilt

#### Scenario: Reopen a square plan

- **WHEN** the quilter opens a version 1 plan
- **THEN** the planner restores the square layout and the original fabrics

#### Scenario: Encounter an unreadable plan

- **WHEN** stored data has an unsupported version or an invalid piece
- **THEN** the application reports the problem and preserves the original data
