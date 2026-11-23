## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Arrange a quilt top

The planner SHALL offer editable blocks made of individual fabric pieces, five labeled fabrics, independent row and column counts from 3 to 9, and a finished block side from 2 to 8 inches. It SHALL allow square patches as well as triangles, rectangular strips, diamonds and other straight-edged pieces. Painting, resizing and applying a pattern SHALL work with pointer and keyboard input.

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

### Requirement: Take a cutting sheet to the sewing table

The planner SHALL provide finished top dimensions, piece quantities by fabric, a row-by-row placement map and cut dimensions that include a quarter-inch seam on every edge. The sheet SHALL be available as plain text.

#### Scenario: Cut a five-by-five topper

- **WHEN** the quilter opens the sheet for five rows and five columns of four-inch finished squares
- **THEN** the top measures 20 by 20 inches and the quantities total 25 squares
- **AND** each square is cut at 4.5 by 4.5 inches

#### Scenario: Cut a pieced block

- **WHEN** the quilter exports the cutting sheet for a design with shaped pieces
- **THEN** each used template has quantities by fabric and dimensions including its seam allowance
- **AND** the placement map identifies the templates, fabrics and block turns

### Requirement: Resume a local plan

The planner SHALL persist completed edits on the current device, validate saved data before loading it, and show a visible status if storage fails. It SHALL load version 1 square plans and version 2 patterned plans.

#### Scenario: Return to yesterday's quilt

- **WHEN** the quilter closes and reopens the application after editing a piece of a patterned quilt
- **THEN** the dimensions, geometry, pattern name and fabrics match the saved quilt

#### Scenario: Reopen a square plan

- **WHEN** the quilter opens a version 1 plan
- **THEN** the planner restores the square layout and the original fabrics

#### Scenario: Encounter an unreadable plan

- **WHEN** stored data has an unsupported version or an invalid piece
- **THEN** the application reports the problem and preserves the original data
