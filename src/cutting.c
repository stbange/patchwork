#include "quilt.h"
#include <SDL3/SDL.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct { double x, y; } Distance;
typedef struct { char *next; size_t left; Preferences prefs; bool complete; } Sheet;
typedef struct { Patch shape; int count[FABRICS]; } CutTemplate;
typedef struct {
    int pattern, turn, width, height;
    int type[MAX_PIECES];
} BlockGuide;

static void append(Sheet *sheet, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int written = vsnprintf(sheet->next, sheet->left, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sheet->left) {
        sheet->complete = false;
        return;
    }
    sheet->next += written;
    sheet->left -= (size_t)written;
}

static const char *caption(const Sheet *sheet, Text key)
{
    return tr(sheet->prefs.language, key);
}

static bool matches(const Patch *a, const Patch *b, int start, int turn)
{
    for (int v = 1; v < a->vertices; ++v) {
        Point p = b->point[(start + v) % b->vertices], origin = b->point[start];
        int x = p.x - origin.x, y = p.y - origin.y;
        for (int i = 0; i < turn; ++i) {
            int next_x = -y;
            y = x;
            x = next_x;
        }
        if (a->point[v].x - a->point[0].x != x || a->point[v].y - a->point[0].y != y) return false;
    }
    return true;
}

static bool same_shape(const Patch *a, const Patch *b)
{
    if (a->vertices != b->vertices) return false;
    for (int turn = 0; turn < 4; ++turn)
        for (int start = 0; start < b->vertices; ++start)
            if (matches(a, b, start, turn)) return true;
    return false;
}

static int length_squared(Point a, Point b)
{
    int dx = b.x - a.x, dy = b.y - a.y;
    return dx * dx + dy * dy;
}

static Text shape_name(const Patch *patch, bool *rectangular)
{
    *rectangular = false;
    if (patch->vertices == 3) return TRIANGLE_NAME;
    if (patch->vertices == 6) return HEXAGON_NAME;
    if (patch->vertices == 8) return OCTAGON_NAME;
    bool even_sides = true, axis_edges = true;
    const Point *p = patch->point;
    for (int v = 0; v < patch->vertices; ++v) {
        Point a = p[v], b = p[(v + 1) % patch->vertices];
        axis_edges = axis_edges && (a.x == b.x || a.y == b.y);
        even_sides = even_sides && length_squared(a, b) == length_squared(p[0], p[1]);
    }
    *rectangular = axis_edges;
    if (axis_edges) return even_sides ? SQUARE_NAME : RECTANGLE_NAME;
    return even_sides ? DIAMOND_NAME : QUADRILATERAL_NAME;
}

static Distance outward(Point a, Point b)
{
    double dx = b.x - a.x, dy = b.y - a.y, length = hypot(dx, dy);
    return (Distance){dy / length, -dx / length};
}

static void coordinate(Sheet *sheet, double x, double y)
{
    char col[NUMBER_BYTES], row[NUMBER_BYTES];
    int scale = unit_scale(sheet->prefs.unit);
    Language language = sheet->prefs.language;
    decimal(col, language, x / scale, 3, false);
    decimal(row, language, y / scale, 3, false);
    append(sheet, " (%s%s %s)", col, language == ENGLISH ? "," : ";", row);
}

static void print_cut(Sheet *sheet, const CutTemplate *item, int number, double scale)
{
    const Patch *patch = &item->shape;
    int n = patch->vertices;
    const Point *p = patch->point;
    Distance cut[MAX_VERTICES], min = {DBL_MAX, DBL_MAX}, max = {-DBL_MAX, -DBL_MAX};
    for (int v = 0; v < n; ++v) {
        Distance before = outward(p[(v + n - 1) % n], p[v]), after = outward(p[v], p[(v + 1) % n]);
        /* Intersect the two edges offset by the physical seam allowance. */
        double seam = SEAM_ALLOWANCE / (1 + before.x * after.x + before.y * after.y);
        cut[v] = (Distance){p[v].x * scale + seam * (before.x + after.x),
                            p[v].y * scale + seam * (before.y + after.y)};
        min.x = fmin(min.x, cut[v].x); max.x = fmax(max.x, cut[v].x);
        min.y = fmin(min.y, cut[v].y); max.y = fmax(max.y, cut[v].y);
    }
    bool rectangular;
    Text name = shape_name(patch, &rectangular);
    char measure[2 * NUMBER_BYTES + 16];
    dimensions(measure, sheet->prefs, max.x - min.x, max.y - min.y, 3, false);
    append(sheet, caption(sheet, TEMPLATE_LINE), number, caption(sheet, name), measure);
    append(sheet, "\n  %s", caption(sheet, CUT_VERB));
    for (int f = 0; f < FABRICS; ++f)
        if (item->count[f]) append(sheet, "  %c %d", 'A' + f, item->count[f]);
    append(sheet, "\n");
    if (!rectangular) {
        append(sheet, "  %s", caption(sheet, CUT_POINTS));
        for (int v = 0; v < n; ++v) coordinate(sheet, cut[v].x - min.x, cut[v].y - min.y);
        append(sheet, "\n");
    }
}

static Patch fitted_patch(BlockGuide group, int index)
{
    Patch patch = patterns[group.pattern].patches[index];
    for (int v = 0; v < patch.vertices; ++v) {
        Point p = turn_point(patch.point[v], group.turn);
        patch.point[v] = (Point){(uint8_t)(p.x * group.width), (uint8_t)(p.y * group.height)};
    }
    return patch;
}

static bool detailed_sheet(const Quilt *q, Sheet *sheet, int pieces)
{
    CutTemplate *templates = SDL_calloc((size_t)pieces + 1, sizeof *templates);
    if (!templates) return false;
    BlockGuide guides[MAX_SIDE * MAX_SIDE];
    int map[MAX_SIDE][MAX_SIDE], groups = 0, types = 1;
    templates[0].shape = fitted_patch((BlockGuide){.width = 2, .height = 2}, 0);

    for (int r = 0; r < q->rows; ++r)
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            BlockBounds b = block_bounds(q, r, c);
            int width = (int)(2 * b.w), height = (int)(2 * b.h), group = 0;
            int count = block_pattern(block)->count;
            while (group < groups) {
                const BlockGuide *g = &guides[group];
                if (g->pattern == block->pattern && g->turn == block->turn &&
                    g->width == width && g->height == height) break;
                ++group;
            }
            if (group == groups) {
                BlockGuide *g = &guides[groups++];
                *g = (BlockGuide){block->pattern, block->turn, width, height, {0}};
                for (int p = 0; p < count; ++p) {
                    Patch shape = fitted_patch(*g, p);
                    int type = 0;
                    while (type < types && !same_shape(&templates[type].shape, &shape)) ++type;
                    if (type == types) templates[types++].shape = shape;
                    g->type[p] = type;
                }
            }
            map[r][c] = group;
            for (int p = 0; p < count; ++p) ++templates[guides[group].type[p]].count[block->fabric[p]];
        }

    double scale = (double)q->size / (2 * BLOCK_UNITS);
    append(sheet, "\n%s (%s)\n\n", caption(sheet, CUT_GUIDE), unit_label(sheet->prefs));
    for (int i = 0; i < types; ++i) {
        int total = 0;
        for (int f = 0; f < FABRICS; ++f) total += templates[i].count[f];
        if (total) print_cut(sheet, &templates[i], i + 1, scale);
    }
    SDL_free(templates);

    append(sheet, "\n%s\n%s\n", caption(sheet, LAYOUT_HEAD), caption(sheet, LAYOUT_GUIDE));
    for (int r = 0; r < q->rows; ++r) {
        append(sheet, "\n");
        append(sheet, caption(sheet, ROW_LABEL), r + 1);
        append(sheet, "\n");
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            const BlockGuide *g = &guides[map[r][c]];
            append(sheet, "  %d. ", c + 1);
            if (block->turn) append(sheet, "[R%d", 90 * (block->turn - 1));
            else append(sheet, "[-");
            for (int p = 0; p < block_pattern(block)->count; ++p)
                append(sheet, " %c%d", 'A' + block->fabric[p], g->type[p] + 1);
            append(sheet, "]  G%d\n", map[r][c] + 1);
        }
    }

    append(sheet, "\n%s (%s)\n", caption(sheet, BLOCK_GUIDE), unit_label(sheet->prefs));
    for (int i = 0; i < groups; ++i) {
        BlockGuide *g = &guides[i];
        const Pattern *pattern = &patterns[g->pattern];
        append(sheet, "\nG%d  %s\n", i + 1, g->pattern ? pattern->name[sheet->prefs.language] :
                                                          caption(sheet, PLAIN_BLOCK));
        for (int p = 0; p < pattern->count; ++p) {
            Patch shape = fitted_patch(*g, p);
            append(sheet, "  ");
            append(sheet, caption(sheet, PIECE_LABEL), p + 1);
            for (int v = 0; v < shape.vertices; ++v)
                coordinate(sheet, shape.point[v].x * scale, shape.point[v].y * scale);
            append(sheet, "\n");
        }
    }
    return sheet->complete;
}

bool cutting_sheet(const Quilt *q, Preferences prefs, char out[SHEET_BYTES])
{
    Sheet sheet = {out, SHEET_BYTES, prefs, true};
    int counts[FABRICS], pieces = 0, blocks = q->rows * q->cols;
    count_pieces(q, counts);
    for (int f = 0; f < FABRICS; ++f) pieces += counts[f];
    char first[2 * NUMBER_BYTES + 16], second[2 * NUMBER_BYTES + 16];
    double w = q->cols * q->size, h = q->rows * q->size;
    dimensions(first, prefs, w, h, 3, true);
    Preferences other = {prefs.language, prefs.unit == INCHES ? CENTIMETRES : INCHES};
    dimensions(second, other, w, h, prefs.unit == INCHES ? 1 : 3, prefs.unit != INCHES);
    append(&sheet, "%s\n\n", caption(&sheet, SHEET_HEADER));
    append(&sheet, caption(&sheet, FINISHED_LINE), first, second);
    append(&sheet, "\n");

    bool mixed = mixed_quilt(q);
    bool detailed = q->pattern || mixed || q->seams != GRID_SEAMS;
    if (detailed) {
        append(&sheet, "%s", quilt_name(q, prefs.language));
        if (q->pattern && !mixed) append(&sheet, " / %s", setting_name[q->setting][prefs.language]);
        if (q->seams) append(&sheet, " / %s", seam_name[q->seams][prefs.language]);
        append(&sheet, "\n");
        append(&sheet, caption(&sheet, CUT_PIECES), pieces, blocks);
        char edge[NUMBER_BYTES];
        decimal(edge, prefs.language, q->size / (double)unit_scale(prefs.unit), 3, true);
        snprintf(second, sizeof second, "%s %s", edge, unit_label(prefs));
        append(&sheet, "\n");
        append(&sheet, caption(&sheet, BLOCK_EDGE), second);
    } else {
        dimensions(first, prefs, q->size + 2 * SEAM_ALLOWANCE, q->size + 2 * SEAM_ALLOWANCE, 4, true);
        append(&sheet, caption(&sheet, CUT_SQUARES), blocks, first);
    }
    append(&sheet, ".\n");
    append(&sheet, caption(&sheet, detailed ? SEAM_PATCH : SEAM_SQUARE), seam_measure(prefs));
    append(&sheet, "\n\n");
    for (int f = 0; f < FABRICS; ++f) {
        if (q->color[f] == original_palette[f])
            append(&sheet, "%c  %-14s %2d\n", 'A' + f, fabric_name[f][prefs.language], counts[f]);
        else
            append(&sheet, "%c  #%06X        %2d\n", 'A' + f, (unsigned)q->color[f], counts[f]);
    }
    if (detailed) return detailed_sheet(q, &sheet, pieces);
    append(&sheet, "\n%s\n", caption(&sheet, LAYOUT_HEAD));
    for (int r = 0; r < q->rows; ++r)
        for (int c = 0; c < q->cols; ++c)
            append(&sheet, "%c%c", 'A' + q->cell[r][c].fabric[0], c + 1 == q->cols ? '\n' : ' ');
    return sheet.complete;
}
