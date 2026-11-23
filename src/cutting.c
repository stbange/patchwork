#include "quilt.h"
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct { double x, y; } Distance;
typedef struct { char *next; size_t left; } Sheet;

static void append(Sheet *sheet, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int written = vsnprintf(sheet->next, sheet->left, format, args);
    va_end(args);
    if (written < 0) return;
    size_t length = (size_t)written < sheet->left ? (size_t)written : sheet->left - 1;
    sheet->next += length;
    sheet->left -= length;
}

static bool same_shape(const Patch *a, const Patch *b)
{
    if (a->vertices != b->vertices) return false;
    Point origin = a->point[0];
    for (int turn = 1; turn <= 4; ++turn)
        for (int start = 0; start < b->vertices; ++start) {
            Point corner = turn_point(b->point[start], turn);
            int v = 1;
            for (; v < a->vertices; ++v) {
                Point rotated = turn_point(b->point[(start + v) % b->vertices], turn);
                if (a->point[v].x - origin.x != rotated.x - corner.x ||
                    a->point[v].y - origin.y != rotated.y - corner.y) break;
            }
            if (v == a->vertices) return true;
        }
    return false;
}

static int length_squared(Point a, Point b)
{
    int dx = b.x - a.x, dy = b.y - a.y;
    return dx * dx + dy * dy;
}

static const char *shape_name(const Patch *patch, bool *rectangular)
{
    *rectangular = false;
    if (patch->vertices == 3) return "Triangle";
    if (patch->vertices == 6) return "Hexagon";
    if (patch->vertices == 8) return "Octagon";
    bool square_sides = true, axis_edges = true;
    const Point *p = patch->point;
    for (int v = 0; v < patch->vertices; ++v) {
        Point a = p[v], b = p[(v + 1) % patch->vertices];
        axis_edges = axis_edges && (a.x == b.x || a.y == b.y);
        square_sides = square_sides && length_squared(a, b) == length_squared(p[0], p[1]);
    }
    *rectangular = axis_edges;
    if (axis_edges) return square_sides ? "Square" : "Rectangle";
    return square_sides ? "Diamond" : "Quadrilateral";
}

static Distance outward(Point a, Point b)
{
    double dx = b.x - a.x, dy = b.y - a.y, length = hypot(dx, dy);
    return (Distance){dy / length, -dx / length};
}

static void print_cut(Sheet *sheet, const Patch *patch, int number, const int counts[FABRICS], double scale)
{
    int n = patch->vertices;
    const Point *p = patch->point;
    Distance cut[MAX_VERTICES], min = {DBL_MAX, DBL_MAX}, max = {-DBL_MAX, -DBL_MAX};
    for (int v = 0; v < n; ++v) {
        Distance before = outward(p[(v + n - 1) % n], p[v]), after = outward(p[v], p[(v + 1) % n]);
        /* Intersect the two edges offset by a quarter inch. */
        double seam = 0.25 / (1 + before.x * after.x + before.y * after.y);
        cut[v] = (Distance){p[v].x * scale + seam * (before.x + after.x),
                            p[v].y * scale + seam * (before.y + after.y)};
        min.x = fmin(min.x, cut[v].x); max.x = fmax(max.x, cut[v].x);
        min.y = fmin(min.y, cut[v].y); max.y = fmax(max.y, cut[v].y);
    }
    bool rectangular;
    const char *name = shape_name(patch, &rectangular);
    append(sheet, "Template %d: %s, %.3f x %.3f in\n  Cut", number, name, max.x - min.x, max.y - min.y);
    for (int f = 0; f < FABRICS; ++f)
        if (counts[f]) append(sheet, "  %c %d", 'A' + f, counts[f]);
    append(sheet, "\n");
    if (!rectangular) {
        append(sheet, "  Cut points:");
        for (int v = 0; v < n; ++v)
            append(sheet, " (%.3f, %.3f)", cut[v].x - min.x, cut[v].y - min.y);
        append(sheet, "\n");
    }
}

static void patterned_sheet(const Quilt *q, Sheet *sheet)
{
    const Pattern *motif = &patterns[q->pattern];
    const Patch *shapes[MAX_PIECES + 1] = {patterns[0].patches};
    int type[MAX_PIECES], types = 1;
    for (int p = 0; p < motif->count; ++p) {
        const Patch *patch = &motif->patches[p];
        int id = 0;
        while (id < types && !same_shape(shapes[id], patch)) ++id;
        if (id == types) shapes[types++] = patch;
        type[p] = id;
    }

    int counts[MAX_PIECES + 1][FABRICS] = {{0}};
    for (int r = 0; r < q->rows; ++r)
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            for (int p = 0; p < block_pattern(q, block)->count; ++p)
                ++counts[block->turn ? type[p] : 0][block->fabric[p]];
        }

    const double scale = (double)q->inches / BLOCK_UNITS;
    append(sheet, "\nCUT TEMPLATES\n"
                  "Dimensions give each template's bounding width and height.\n"
                  "For shaped pieces, measure (x, y) in inches from the upper-left of that box.\n"
                  "Join the cut points in order and close the outline.\n\n");
    for (int i = 0; i < types; ++i) {
        int total = 0;
        for (int f = 0; f < FABRICS; ++f) total += counts[i][f];
        if (total) print_cut(sheet, shapes[i], i + 1, counts[i], scale);
    }

    append(sheet, "\nLAYOUT (top row first)\n"
                  "Read the blocks left to right. R0, R90, R180 and R270 turn the block clockwise.\n"
                  "A dash marks a plain square. A2 means fabric A, template 2.\n");
    for (int r = 0; r < q->rows; ++r) {
        append(sheet, "\nRow %d\n", r + 1);
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            append(sheet, "  %d. ", c + 1);
            if (block->turn) append(sheet, "[R%d", 90 * (block->turn - 1));
            else append(sheet, "[-");
            for (int p = 0; p < block_pattern(q, block)->count; ++p) {
                int id = block->turn ? type[p] : 0;
                append(sheet, " %c%d", 'A' + block->fabric[p], id + 1);
            }
            append(sheet, "]\n");
        }
    }

    append(sheet, "\nBASE BLOCK PIECES (in layout order)\n"
                  "Sewing-line points in inches from the upper-left of the finished block at R0.\n");
    for (int p = 0; p < motif->count; ++p) {
        const Patch *patch = &motif->patches[p];
        append(sheet, "  Piece %d:", p + 1);
        for (int v = 0; v < patch->vertices; ++v) {
            Point point = patch->point[v];
            append(sheet, " (%.3f, %.3f)", point.x * scale, point.y * scale);
        }
        append(sheet, "\n");
    }
}

void cutting_sheet(const Quilt *q, char out[SHEET_BYTES])
{
    Sheet sheet = {out, SHEET_BYTES};
    int counts[FABRICS], w = q->cols * q->inches, h = q->rows * q->inches;
    count_pieces(q, counts);
    append(&sheet, "PATCHWORK / CUTTING SHEET\n\nFinished top: %d x %d in (%.1f x %.1f cm)\n",
           w, h, w * 2.54, h * 2.54);
    if (q->pattern) {
        int total = 0;
        for (int f = 0; f < FABRICS; ++f) total += counts[f];
        append(&sheet, "%s / %s\nCut %d pieces for %d blocks. Each finished block is %d in square.\n"
                       "Includes a 1/4 in seam allowance on every piece edge.\n\n",
               patterns[q->pattern].name, setting_name[q->setting], total, q->rows * q->cols, q->inches);
    } else {
        append(&sheet, "Cut %d squares at %d.5 x %d.5 in.\nIncludes a 1/4 in seam on all four edges.\n\n",
               q->rows * q->cols, q->inches, q->inches);
    }
    for (int f = 0; f < FABRICS; ++f)
        append(&sheet, "%c  %-14s %2d\n", 'A' + f, fabric_name[f], counts[f]);
    if (q->pattern) {
        patterned_sheet(q, &sheet);
    } else {
        append(&sheet, "\nLAYOUT (top row first)\n");
        for (int r = 0; r < q->rows; ++r)
            for (int c = 0; c < q->cols; ++c)
                append(&sheet, "%c%c", 'A' + q->cell[r][c].fabric[0], c + 1 == q->cols ? '\n' : ' ');
    }
}
