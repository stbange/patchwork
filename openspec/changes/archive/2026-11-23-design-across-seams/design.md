## Context

The planner stores individual block pieces and five starter fabrics. Whole-quilt construction also needs pieces that cross the original block boundaries.

## Goals / Non-Goals

Support a fabric basket labeled A–Z, nonrectangular tessellations and recognizable organic motifs. Keep a mixed block top accessible underneath appliqué.

## Decisions

- Keep the letter on every fabric stable. Named five-color combinations update A–E. Adding a sixth fabric opens the same RGB editor used for the first five.
- A quilt has a foundation and can carry one appliqué design. Wholecloth, interlocking hexagons, tumbling blocks and curved clamshells provide foundations. Flowering Vine, Koi Pond and Ribbon Lattice provide appliqué. A quilter can preview these against her current quilt or choose wholecloth.
- Generate outlines in quilt coordinates. Use stable piece identifiers for painting and saving. At a woven crossing, the top segment refers to the same physical ribbon throughout.
- Full-size A4 tracing pages share an origin with the color map. Print foundation stitching lines and seam-inclusive cutting outlines. Raw-edge appliqué uses the outline as the cut line. Supply a calibration mark and tile coordinates.
- Keep versions 1–3 readable. Version 4 stores the fabric count, extra colors, quilt-wide design choices and individual fabric assignments.

## Risks / Trade-offs

Curved outlines need the same sampling for hit tests, previews and tracing. Tests check area and shared seams, occlusion and independent color changes. Tiled patterns use more paper for larger quilts.
