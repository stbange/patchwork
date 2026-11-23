#include "whole.h"
#include <math.h>
#include <string.h>

/* Two sides sampled nine times per block, with room for clipping intersections. */
_Static_assert(2 * (9 * MAX_SIDE + 1) + 8 <= OUTLINE_POINTS, "Ribbon outline capacity");

static const double pi = 3.141592653589793, epsilon = 1e-9;
const Text whole_name[WHOLE_COUNT] = {DESIGN_NAME, CLOTH_NAME, HEX_FIELD_NAME, CUBES_NAME,
                                      SHELLS_NAME, VINE_NAME,  KOI_NAME,       RIBBONS_NAME};
const Text whole_help[WHOLE_COUNT] = {CHOOSE_FABRIC, CLOTH_HELP, HEX_FIELD_HELP, CUBES_HELP,
                                      SHELLS_HELP,   VINE_HELP,  KOI_HELP,       RIBBONS_HELP};
typedef struct {
    const Quilt *q;
    Layer layer;
    PieceVisitor visit;
    void *data;
} Walk;

static double cross(XY a, XY b, XY c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool same_point(XY a, XY b)
{
    return fabs(a.x - b.x) < epsilon && fabs(a.y - b.y) < epsilon;
}

static double distance_to_edge(XY p, int edge, const Quilt *q)
{
    switch (edge) {
    case 0:
        return p.x;
    case 1:
        return q->cols - p.x;
    case 2:
        return p.y;
    default:
        return q->rows - p.y;
    }
}

/* Trim perimeter pieces to the finished top, retaining the actual outline for cutting. */
static void clip(Outline *s, const Quilt *q, int edge)
{
    XY out[OUTLINE_POINTS], a = s->p[s->n - 1];
    double da = distance_to_edge(a, edge, q);
    int n = 0;
    for (int i = 0; i < s->n; ++i) {
        XY b = s->p[i];
        double db = distance_to_edge(b, edge, q);
        if ((da < 0) != (db < 0)) {
            double t = da / (da - db);
            out[n++] = (XY){a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
        }
        if (db >= 0) out[n++] = b;
        a = b;
        da = db;
    }
    memcpy(s->p, out, (size_t)n * sizeof *out);
    s->n = n;
}

static void emit(Walk *w, Outline *s)
{
    for (int edge = 0; edge < 4 && s->n >= 3; ++edge) clip(s, w->q, edge);
    int n = 0;
    for (int i = 0; i < s->n; ++i)
        if (!n || !same_point(s->p[n - 1], s->p[i])) s->p[n++] = s->p[i];
    if (n && same_point(s->p[0], s->p[n - 1])) --n;
    if (n < 3) return;
    double area = 0;
    for (int i = 1; i < n - 1; ++i) area += cross(s->p[0], s->p[i], s->p[i + 1]);
    if (fabs(area) < epsilon) return;
    if (area < 0)
        for (int i = 0; i < n / 2; ++i) {
            XY point = s->p[i];
            s->p[i] = s->p[n - 1 - i];
            s->p[n - 1 - i] = point;
        }
    s->n = n;
    s->base_fabric = s->fabric;
    int dye = w->q->dye[w->layer][s->id];
    if (dye) s->fabric = dye - 1;
    w->visit(s, w->data);
}

static void hexagons(Walk *w, bool cubes)
{
    double radius = 1 / sqrt(3.0), pitch = 1.5 * radius;
    for (int row = 0; row * pitch - radius < w->q->rows; ++row)
        for (int col = 0; col <= w->q->cols; ++col) {
            double x = col + 0.5 * (row % 2), y = row * pitch;
            int id = 3 * (row * (MAX_SIDE + 1) + col);
            Outline hex = {.id = id,
                           .fabric = (col + 2 * row) % STARTER_FABRICS,
                           .name = HEXAGON_NAME,
                           .n = 6,
                           .p = {{x, y - radius},
                                 {x + .5, y - radius / 2},
                                 {x + .5, y + radius / 2},
                                 {x, y + radius},
                                 {x - .5, y + radius / 2},
                                 {x - .5, y - radius / 2}}};
            if (!cubes) {
                emit(w, &hex);
                continue;
            }
            const int fabric[] = {3, 1, 0};
            for (int face = 0; face < 3; ++face) {
                int v = 2 * face + 1;
                Outline diamond = {.id = id + face,
                                   .fabric = fabric[face],
                                   .name = DIAMOND_NAME,
                                   .n = 4,
                                   .p = {{x, y}, hex.p[v], hex.p[(v + 1) % 6], hex.p[(v + 2) % 6]}};
                emit(w, &diamond);
            }
        }
}

static void clamshells(Walk *w)
{
    enum { HALF_ARC = 12 };
    for (int row = 0; row <= 2 * w->q->rows; ++row)
        for (int col = -1; col < w->q->cols; ++col) {
            double x = col + 0.5 * (row % 2), y = row * 0.5;
            Outline s = {.id = row * (MAX_SIDE + 1) + col + 1,
                         .name = SHELL_PIECE,
                         .fabric = (row + 2 * (col + 1)) % STARTER_FABRICS};
            for (int a = 0; a <= 2 * HALF_ARC; ++a) {
                double t = pi * a / (2 * HALF_ARC);
                s.p[s.n++] = (XY){x + .5 - .5 * cos(t), y - .5 * sin(t)};
            }
            for (int a = 1; a <= HALF_ARC; ++a) {
                double t = pi * a / (2 * HALF_ARC);
                s.p[s.n++] = (XY){x + 1 - .5 * sin(t), y + .5 - .5 * cos(t)};
            }
            for (int a = 1; a < HALF_ARC; ++a) {
                double t = pi * a / (2 * HALF_ARC);
                s.p[s.n++] = (XY){x + .5 * cos(t), y + .5 - .5 * sin(t)};
            }
            emit(w, &s);
        }
}

static double stem_y(double x, int row, double middle)
{
    return middle + .22 * sin(pi * (x + .35 * row));
}

static void band(Walk *w, int id, int fabric, int row, double middle, double start, double end, bool vertical,
                 bool vine, bool bridge)
{
    int steps = (int)lround((end - start) * 9);
    double half = vine ? .052 : .115;
    Outline s = {.id = id, .name = vine ? STEM_PIECE : BAND_PIECE, .fabric = fabric, .bridge = bridge};
    for (int side = -1; side <= 1; side += 2)
        for (int j = 0; j <= steps; ++j) {
            int i = side < 0 ? j : steps - j;
            double x = start + (end - start) * i / steps;
            double y = vine ? stem_y(x, row, middle) : middle + .14 * sin(pi * (x - .5));
            y += side * half;
            s.p[s.n++] = vertical ? (XY){y, x} : (XY){x, y};
        }
    emit(w, &s);
}

/* A small, normalized sewing outline built from cubic Bézier segments. */
static void cubic(Outline *s, XY a, XY b, XY end)
{
    XY start = s->p[s->n - 1];
    for (int i = 1; i <= 10; ++i) {
        double t = i / 10.0, u = 1 - t;
        s->p[s->n++] = (XY){u * u * u * start.x + 3 * u * u * t * a.x + 3 * u * t * t * b.x + t * t * t * end.x,
                            u * u * u * start.y + 3 * u * u * t * a.y + 3 * u * t * t * b.y + t * t * t * end.y};
    }
}

static void place_shape(Walk *w, Outline *s, double x, double y, double angle, double scale)
{
    double c = cos(angle) * scale, t = sin(angle) * scale;
    for (int i = 0; i < s->n; ++i) {
        XY p = s->p[i];
        s->p[i] = (XY){x + c * p.x - t * p.y, y + t * p.x + c * p.y};
    }
    emit(w, s);
}

static void rosette(Walk *w, int id, int fabric, Text name, double x, double y, double radius, int petals,
                    double angle)
{
    Outline s = {.id = id, .name = name, .fabric = fabric};
    for (int j = 0; j < 60; ++j) {
        double t = 2 * pi * j / 60, r = petals ? .78 + .22 * cos(petals * t) : 1;
        s.p[s.n++] = (XY){r * cos(t), r * sin(t)};
    }
    place_shape(w, &s, x, y, angle, radius);
}

static void vines(Walk *w)
{
    int rows = (w->q->rows + 1) / 2;
    for (int r = 0; r < rows; ++r) {
        double middle = (r + .5) * w->q->rows / rows;
        int first = r * (1 + 4 * MAX_SIDE);
        band(w, first, 2, r, middle, 0, w->q->cols, false, true, false);
        if (r + 1 == rows) continue;
        for (int c = r % 2; c + 1 < w->q->cols; c += 2) {
            double x = c + .35, top = stem_y(x, r, middle);
            double end = stem_y(x + 1, r + 1, middle + (double)w->q->rows / rows);
            Outline shoot = {
                .id = first + 1 + 3 * MAX_SIDE + c, .name = SHOOT_PIECE, .fabric = 2, .n = 1, .p = {{x, top}}};
            cubic(&shoot, (XY){x + .74, top + .26}, (XY){x + .08, end - .38}, (XY){x + 1, end});
            cubic(&shoot, (XY){x - .10, end - .34}, (XY){x + .59, top + .25}, (XY){x, top});
            emit(w, &shoot);
        }
    }
    for (int r = 0; r < rows; ++r) {
        double middle = (r + .5) * w->q->rows / rows;
        int first = r * (1 + 4 * MAX_SIDE);
        for (int c = 0; c < w->q->cols; ++c) {
            int id = first + 1 + 3 * c, side = (r + c) % 2 ? 1 : -1;
            double x = c + .19, y = stem_y(x, r, middle);
            Outline leaf = {.id = id, .name = LEAF_PIECE, .fabric = 2, .n = 1, .p = {{0, 0}}};
            cubic(&leaf, (XY){.10, -.30}, (XY){.44, -.27}, (XY){.64, -.11});
            cubic(&leaf, (XY){.40, -.06}, (XY){.27, .15}, (XY){0, 0});
            place_shape(w, &leaf, x, y, side * .76, 1);
            x = c + .72;
            y = stem_y(x, r, middle) - side * .17;
            rosette(w, id + 1, (c + r) % 3 ? 1 : 3, FLOWER_PIECE, x, y, .30, 5, .3 * c - r);
            rosette(w, id + 2, 4, CENTER_PIECE, x, y, .085, 0, 0);
        }
    }
}

static void pond(Walk *w)
{
    int rows = (w->q->rows + 1) / 2, cols = (w->q->cols + 1) / 2;
    double dx = (double)w->q->cols / cols, dy = (double)w->q->rows / rows;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            int id = 6 * (r * MAX_SIDE + c);
            double angle = (r + c) % 2 ? pi - .32 : .40;
            double x = (c + .5) * dx, y = (r + .53) * dy, scale = fmin(dx, dy) * .54;
            Outline pad = {.id = id, .fabric = 2, .name = LILY_PIECE, .n = 1, .p = {{-.06, .03}}};
            cubic(&pad, (XY){-.12, -.06}, (XY){-.52, -.21}, (XY){-.51, -.26});
            cubic(&pad, (XY){-.28, -.68}, (XY){.54, -.52}, (XY){.55, -.01});
            cubic(&pad, (XY){.60, .54}, (XY){-.40, .62}, (XY){-.54, .13});
            place_shape(w, &pad, (c + .25) * dx, (r + .23) * dy, angle, scale * .60);
            Outline tail = {.id = id + 1, .fabric = 4, .name = TAIL_PIECE, .n = 1, .p = {{-.76, -.04}}};
            cubic(&tail, (XY){-.84, -.14}, (XY){-.96, -.22}, (XY){-.98, -.35});
            cubic(&tail, (XY){-.73, -.32}, (XY){-.62, -.16}, (XY){-.44, -.05});
            cubic(&tail, (XY){-.60, .03}, (XY){-.72, .22}, (XY){-.98, .22});
            cubic(&tail, (XY){-.98, .12}, (XY){-.86, .04}, (XY){-.76, -.04});
            place_shape(w, &tail, x, y, angle, scale);
            Outline fin = {.id = id + 2, .fabric = 4, .name = FIN_PIECE, .n = 1, .p = {{.32, .02}}};
            cubic(&fin, (XY){.35, .27}, (XY){.18, .50}, (XY){-.10, .54});
            cubic(&fin, (XY){.03, .31}, (XY){-.08, .15}, (XY){.08, .02});
            place_shape(w, &fin, x, y, angle, scale);
            Outline fish = {
                .id = id + 3, .fabric = (r + c) % 2 ? 0 : 1, .name = FISH_PIECE, .n = 1, .p = {{-.70, -.05}}};
            cubic(&fish, (XY){-.26, -.08}, (XY){-.01, -.51}, (XY){.53, -.35});
            cubic(&fish, (XY){.93, -.23}, (XY){.99, .06}, (XY){.75, .17});
            cubic(&fish, (XY){.10, .48}, (XY){-.09, .02}, (XY){-.65, .05});
            place_shape(w, &fish, x, y, angle, scale);
            Outline mark = {
                .id = id + 4, .fabric = (r + c) % 2 ? 1 : 0, .name = MARK_PIECE, .n = 1, .p = {{.03, -.12}}};
            cubic(&mark, (XY){.10, -.27}, (XY){.43, -.31}, (XY){.46, -.17});
            cubic(&mark, (XY){.48, -.04}, (XY){.27, -.05}, (XY){.22, .09});
            cubic(&mark, (XY){.05, .17}, (XY){-.06, .04}, (XY){.03, -.12});
            place_shape(w, &mark, x, y, angle, scale);
            double ex = x + scale * (.71 * cos(angle) + .13 * sin(angle)),
                   ey = y + scale * (.71 * sin(angle) - .13 * cos(angle));
            rosette(w, id + 5, 3, EYE_PIECE, ex, ey, .066 * scale, 0, 0);
        }
}

static void ribbons(Walk *w)
{
    for (int r = 0; r < w->q->rows; ++r) band(w, r, r % 2 ? 1 : 3, r, r + .5, 0, w->q->cols, false, false, false);
    for (int c = 0; c < w->q->cols; ++c)
        band(w, MAX_SIDE + c, c % 2 ? 4 : 2, c, c + .5, 0, w->q->rows, true, false, false);
    /* Redraw a portion of the original weft at alternate intersections. */
    for (int r = 0; r < w->q->rows; ++r)
        for (int c = r % 2; c < w->q->cols; c += 2)
            band(w, r, r % 2 ? 1 : 3, r, r + .5, c + 2.0 / 9, c + 7.0 / 9, false, false, true);
}

void set_whole_design(Quilt *q, Layer layer, WholeDesign design)
{
    if ((layer == FOUNDATION && (design < BLOCK_TOP || design >= VINE)) ||
        (layer == APPLIQUE && design != BLOCK_TOP && (design < VINE || design >= WHOLE_COUNT)))
        return;
    uint8_t *choice = layer == FOUNDATION ? &q->foundation : &q->applique;
    if (*choice == design) return;
    *choice = (uint8_t)design;
    memset(q->dye[layer], 0, sizeof q->dye[layer]);
}

void visit_layer(const Quilt *q, Layer layer, PieceVisitor visit, void *context)
{
    Walk w = {q, layer, visit, context};
    int design = layer == FOUNDATION ? q->foundation : q->applique;
    switch (design) {
    case WHOLECLOTH: {
        Outline s = {
            .name = CLOTH_NAME, .fabric = 0, .n = 4, .p = {{0, 0}, {q->cols, 0}, {q->cols, q->rows}, {0, q->rows}}};
        emit(&w, &s);
        break;
    }
    case HEX_FIELD:
    case TUMBLING:
        hexagons(&w, design == TUMBLING);
        break;
    case CLAMSHELL:
        clamshells(&w);
        break;
    case VINE:
        vines(&w);
        break;
    case KOI:
        pond(&w);
        break;
    case RIBBONS:
        ribbons(&w);
        break;
    }
}

bool inside_outline(const Outline *s, XY p)
{
    bool inside = false;
    for (int i = 0, j = s->n - 1; i < s->n; j = i++) {
        XY a = s->p[j], b = s->p[i];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
    }
    return inside;
}

typedef struct {
    XY p;
    int id;
} Hit;
static void hit(const Outline *s, void *data)
{
    Hit *h = data;
    if (inside_outline(s, h->p)) h->id = s->id;
}

int hit_whole(const Quilt *q, Layer layer, XY p)
{
    Hit h = {p, -1};
    visit_layer(q, layer, hit, &h);
    return h.id;
}

typedef struct {
    int id, previous, next, first, last, base;
} Selection;
static void find_piece(const Outline *s, void *data)
{
    Selection *k = data;
    if (s->bridge) return;
    if (k->first < 0) k->first = s->id;
    if (k->last == k->id && k->next < 0) k->next = s->id;
    if (s->id == k->id) {
        k->previous = k->last;
        k->base = s->base_fabric;
    }
    k->last = s->id;
}

int next_whole(const Quilt *q, Layer layer, int current, int direction)
{
    Selection k = {current, -1, -1, -1, -1, -1};
    visit_layer(q, layer, find_piece, &k);
    if (direction < 0) return k.previous >= 0 ? k.previous : k.last;
    return k.next >= 0 ? k.next : k.first;
}

bool paint_whole(Quilt *q, Layer layer, int id, int fabric)
{
    if (fabric < 0 || fabric >= q->fabrics || id < 0 || id >= LAYER_SLOTS) return false;
    Selection k = {id, -1, -1, -1, -1, -1};
    visit_layer(q, layer, find_piece, &k);
    if (k.base < 0) return false;
    uint8_t dye = (uint8_t)(fabric == k.base ? 0 : 1 + fabric);
    if (q->dye[layer][id] == dye) return false;
    q->dye[layer][id] = dye;
    return true;
}

XY outline_label(const Outline *s)
{
    /* Center the label in the widest interior span halfway down the piece. */
    double top = s->p[0].y, bottom = top, cuts[OUTLINE_POINTS];
    for (int i = 1; i < s->n; ++i) {
        top = fmin(top, s->p[i].y);
        bottom = fmax(bottom, s->p[i].y);
    }
    double y = (top + bottom) / 2, width = -1;
    XY label = s->p[0];
    int n = 0;
    for (int i = 0, j = s->n - 1; i < s->n; j = i++) {
        XY a = s->p[j], b = s->p[i];
        if ((a.y > y) == (b.y > y)) continue;
        double x = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
        int k = n++;
        while (k > 0 && cuts[k - 1] > x) {
            cuts[k] = cuts[k - 1];
            --k;
        }
        cuts[k] = x;
    }
    for (int i = 0; i + 1 < n; i += 2)
        if (cuts[i + 1] - cuts[i] > width) {
            width = cuts[i + 1] - cuts[i];
            label = (XY){(cuts[i] + cuts[i + 1]) / 2, y};
        }
    return label;
}

/* Ear clipping handles the inward curves of shells, fins, leaves and ribbons. */
int outline_mesh(const Outline *s, int indices[3 * (OUTLINE_POINTS - 2)])
{
    int vertex[OUTLINE_POINTS], n = s->n, written = 0;
    for (int i = 0; i < n; ++i) vertex[i] = i;
    while (n >= 3) {
        bool found = false;
        for (int i = 0; i < n; ++i) {
            int a = vertex[(i + n - 1) % n], b = vertex[i], c = vertex[(i + 1) % n];
            double area = cross(s->p[a], s->p[b], s->p[c]);
            if (area < -epsilon) continue;
            bool ear = true;
            if (area > epsilon) {
                for (int j = 0; j < n; ++j) {
                    int p = vertex[j];
                    if (p == a || p == b || p == c) continue;
                    if (cross(s->p[a], s->p[b], s->p[p]) >= -epsilon &&
                        cross(s->p[b], s->p[c], s->p[p]) >= -epsilon &&
                        cross(s->p[c], s->p[a], s->p[p]) >= -epsilon) {
                        ear = false;
                        break;
                    }
                }
            }
            if (!ear) continue;
            if (area > epsilon) {
                indices[written++] = a;
                indices[written++] = b;
                indices[written++] = c;
            }
            memmove(vertex + i, vertex + i + 1, (size_t)(n - i - 1) * sizeof *vertex);
            --n;
            found = true;
            break;
        }
        if (!found) break;
    }
    return written;
}

int cut_outline(const Outline *s, double allowance, XY *cut)
{
    int n = 0;
    for (int i = 0; i < s->n; ++i) {
        XY a = s->p[(i + s->n - 1) % s->n], b = s->p[i], c = s->p[(i + 1) % s->n];
        double ab = hypot(b.x - a.x, b.y - a.y), bc = hypot(c.x - b.x, c.y - b.y);
        XY before = {(b.y - a.y) / ab, (a.x - b.x) / ab}, after = {(c.y - b.y) / bc, (b.x - c.x) / bc};
        double dot = before.x * after.x + before.y * after.y;
        if (dot < -.5 && cross(a, b, c) > 0) {
            /* Round a sharp outer tip at the seam radius, avoiding a long, fragile miter. */
            double start = atan2(before.y, before.x);
            double angle = acos(fmax(-1, dot));
            for (int j = 0; j <= CUT_ARC_STEPS; ++j) {
                double t = start + angle * j / CUT_ARC_STEPS;
                cut[n++] = (XY){b.x + allowance * cos(t), b.y + allowance * sin(t)};
            }
        } else {
            double offset = allowance / (1 + dot);
            cut[n++] = (XY){b.x + offset * (before.x + after.x), b.y + offset * (before.y + after.y)};
        }
    }
    return n;
}
