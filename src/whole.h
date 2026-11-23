#ifndef PATCHWORK_WHOLE_H
#define PATCHWORK_WHOLE_H

#include "quilt.h"

/* Coordinates are in finished block widths, measured from the top left of the quilt. */
enum { OUTLINE_POINTS = 192, CUT_ARC_STEPS = 12, MAX_CUT_POINTS = OUTLINE_POINTS * (1 + CUT_ARC_STEPS) };
typedef struct {
    double x, y;
} XY;
typedef struct {
    int id, fabric, base_fabric, n;
    Text name;
    bool bridge;
    XY p[OUTLINE_POINTS];
} Outline;
typedef void (*PieceVisitor)(const Outline *, void *);

extern const Text whole_name[WHOLE_COUNT];
extern const Text whole_help[WHOLE_COUNT];
void set_whole_design(Quilt *q, Layer layer, WholeDesign design);
void visit_layer(const Quilt *q, Layer layer, PieceVisitor visit, void *context);
bool paint_whole(Quilt *q, Layer layer, int id, int fabric);
int hit_whole(const Quilt *q, Layer layer, XY point);
int next_whole(const Quilt *q, Layer layer, int current, int direction);
bool inside_outline(const Outline *piece, XY point);
XY outline_label(const Outline *piece);
int outline_mesh(const Outline *piece, int triangles[3 * (OUTLINE_POINTS - 2)]);
int cut_outline(const Outline *piece, double allowance, XY *cut);
#endif
