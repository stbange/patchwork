## Context

Imagine making a 20-inch table topper from scraps. The quilter chooses five real fabrics and matches each to a named, patterned swatch. The initial five-by-five arrangement is ready to change. At the cutting mat, she can match letters on the sheet to her piles of fabric and sew one row at a time.

## Goals

A first-time user should be able to change the center square, undo the change and find the cutting dimensions without knowing quilting software. The prototype models a pieced top made from squares. Backing, batting, binding and fabric yardage belong to different calculations.

## Decisions

- **A design wall and a cutting sheet.** The wall keeps a large quilt beside the fabric basket. Letters, different motifs and keyboard shortcuts supplement color. The sheet groups squares by fabric and shows placement from top to bottom. Keep all controls visible at the reference desktop size.
- **Real dimensions.** Count rows and columns independently. A finished 4-inch square needs a 4½-inch cut square for a ¼-inch seam on each edge. A five-by-five layout finishes at 20 inches square. Its 25 cut squares already include the seam allowance. The sheet provides exact inch instructions and a metric finished-size reference. [Worked example of square piecing](https://weallsew.com/postage-stamp-patchwork-pillow/)
- **Small C implementation.** Store a bounded grid, its dimensions and the finished patch size. Undo stores snapshots of this small value. SDL handles input, rendering and the event loop. Shared C derives every quantity shown on either screen. Isolate browser storage and downloads at the platform boundary. Use labeled procedural fabric motifs and bundled open fonts. [SDL application callbacks](https://wiki.libsdl.org/SDL3/README-main-functions) · [Ponytail](https://github.com/DietrichGebert/ponytail)
- **Local projects.** An edit updates the current plan. Desktop builds replace a versioned file using a temporary file in the same directory. The browser stores that identical representation under an origin-local key. Parse into a temporary quilt, then replace the active quilt only after validation. Surface storage errors and preserve unreadable originals.
- **Portability and verification.** Build with CMake, a desktop C compiler on Windows or Linux, and Emscripten for the browser. Measure executable lines from first-party C during the unit suite alone. Run a separate process-level desktop journey and a real-browser journey against the compiled application.

## Questions for the first sewing session

Ask the quilter to replace a patch with a fabric from her scrap basket, close and reopen the planner, then point to the number and size of squares she would cut. If she wants to see the topper in her room, try an optional 3D preview using the existing grid as a texture on a table. If she needs to identify an exact print, try importing a fabric photograph first.

The author subsequently clarified that “OTL” referred to OpenGL for graphics. The design wall and cutting sheet use SDL's OpenGL renderer on desktops and WebGL in browsers. [Current quilt-planning specification](../../../specs/quilt-planning/spec.md)
