#include "app.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "stb_truetype.h"

#ifdef __EMSCRIPTEN__
extern void browser_describe_view(const char *description);
#endif

typedef struct {
    SDL_Texture *texture;
    stbtt_bakedchar glyph[95];
} Face;

struct View {
    SDL_Renderer *renderer;
    Face body, title;
    SDL_Texture *fabric[FABRICS];
};

static const unsigned ink = 0x263e35, muted = 0x56665b, paper = 0xfaf8f2,
    rule = 0xd6d4c6, green = 0x325545;
static const unsigned fabric_rgb[FABRICS] = {0xeee0bf, 0xb86761, 0x8a9b78, 0x4c6171, 0xd5ab62};
const SDL_FRect design_mat = {36, 165, 670, 466};
const SDL_FRect control_box[CONTROL_COUNT] = {
    [DESIGN] = {376, 12, 125, 48}, [SHEET] = {509, 12, 139, 48}, [LIBRARY] = {656, 12, 133, 48},
    [UNDO] = {853, 19, 89, 36}, [REDO] = {950, 19, 89, 36},
    [SWATCH_A] = {754, 157, 286, 44}, [SWATCH_B] = {754, 205, 286, 44},
    [SWATCH_C] = {754, 253, 286, 44}, [SWATCH_D] = {754, 301, 286, 44},
    [SWATCH_E] = {754, 349, 286, 44},
    [COL_LESS] = {914, 475, 35, 34}, [COL_MORE] = {1005, 475, 35, 34},
    [ROW_LESS] = {914, 523, 35, 34}, [ROW_MORE] = {1005, 523, 35, 34},
    [PATCH_LESS] = {914, 571, 35, 34}, [PATCH_MORE] = {1005, 571, 35, 34},
    [SHARE] = {753, 560, 287, 48},
    [FILTER_ALL] = {36, 150, 95, 37}, [FILTER_TRADITIONAL] = {143, 150, 122, 37},
    [FILTER_MODERN] = {277, 150, 106, 37},
    [PREV_PAGE] = {36, 620, 101, 37}, [NEXT_PAGE] = {203, 620, 85, 37},
    [APPLY] = {705, 615, 335, 42}
};

SDL_FRect pattern_card(int slot)
{
    return (SDL_FRect){36 + (slot % CARDS_PER_ROW) * 207, 201 + (slot / CARDS_PER_ROW) * 191, 194, 178};
}

SDL_FRect setting_box(int setting)
{
    return (SDL_FRect){705 + (setting % 2) * 171, 440 + (setting / 2) * 42, 164, 36};
}

static bool load_face(View *v, Face *face, const char *base, const char *name)
{
    char *path = NULL;
    if (!base || SDL_asprintf(&path, "%sassets/%s", base, name) < 0) return false;
    unsigned char *font = SDL_LoadFile(path, NULL);
    SDL_free(path);
    if (!font) return false;

    const int width = 512, height = 512;
    unsigned char *alpha = SDL_malloc((size_t)width * height);
    SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    if (alpha && surface && stbtt_BakeFontBitmap(font, 0, 54, alpha, width, height,
                                                 32, 95, face->glyph) > 0) {
        for (int y = 0; y < height; ++y) {
            Uint32 *pixels = (Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch);
            for (int x = 0; x < width; ++x)
                pixels[x] = ((Uint32)alpha[y * width + x] << 24) | 0xffffff;
        }
        face->texture = SDL_CreateTextureFromSurface(v->renderer, surface);
    }
    SDL_DestroySurface(surface);
    SDL_free(alpha);
    SDL_free(font);
    return face->texture != NULL;
}

static int motif(int f, int x, int y)
{
    int dx = x % 16 - 8, dy = y % 16 - 8;
    switch (f) {
    case 0: return ((x + 2 * y) % 17 == 0) * 8;
    case 1: /* A block-printed flower, offset on alternate rows. */
        dx = (x + (y / 16) * 8) % 16 - 8;
        if (abs(dx) + abs(dy) == 0) return -29;
        return ((abs(dx) == 2 && abs(dy) <= 1) || (abs(dy) == 2 && abs(dx) <= 1)) * 48;
    case 2: /* A stem with a pair of leaves. */
        return ((dx == -dy / 2 && abs(dy) < 5) || (dy == -2 && dx >= 1 && dx < 5) ||
                (dy == 1 && dx < 0 && dx > -4)) * -33;
    case 3: return ((x + y) % 16 < 2) * 29 + ((x + y) % 16 == 4) * 14;
    default: return (x % 16 < 3) * -17 + (y % 16 < 3) * 20;
    }
}

static SDL_Texture *fabric_tile(SDL_Renderer *renderer, int f)
{
    /* WebGL 1 requires power-of-two textures for repeating fabric patterns. */
    SDL_Surface *s = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return NULL;
    for (int y = 0; y < s->h; ++y) {
        Uint32 *pixels = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
        for (int x = 0; x < s->w; ++x) {
            int weave = (x % 3 == 0) * 4 - (y % 3 == 0) * 5 + (x * 13 + y * 7) % 5 - 2;
            int offset = motif(f, x, y) + weave;
            unsigned color = 0xff000000;
            for (int shift = 0; shift <= 16; shift += 8) {
                int channel = (int)((fabric_rgb[f] >> shift) & 255);
                color |= (unsigned)SDL_clamp(channel + offset, 0, 255) << shift;
            }
            pixels[x] = color;
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, s);
    SDL_DestroySurface(s);
    return texture;
}

View *open_view(SDL_Renderer *renderer, const char *base)
{
    View *v = SDL_calloc(1, sizeof *v);
    if (!v) return NULL;
    v->renderer = renderer;
    bool ok = load_face(v, &v->body, base, "DM-Sans.ttf") &&
              load_face(v, &v->title, base, "InstrumentSerif-Regular.ttf");
    for (int f = 0; ok && f < FABRICS; ++f) ok = (v->fabric[f] = fabric_tile(renderer, f)) != NULL;
    if (ok) return v;
    close_view(v);
    return NULL;
}

void close_view(View *v)
{
    if (!v) return;
    SDL_DestroyTexture(v->body.texture);
    SDL_DestroyTexture(v->title.texture);
    for (int f = 0; f < FABRICS; ++f) SDL_DestroyTexture(v->fabric[f]);
    SDL_free(v);
}

static void color(View *v, unsigned rgb)
{
    SDL_SetRenderDrawColor(v->renderer, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, 255);
}

static void box(View *v, SDL_FRect b, unsigned rgb)
{
    color(v, rgb);
    SDL_RenderFillRect(v->renderer, &b);
}

static void border(View *v, SDL_FRect b, unsigned rgb)
{
    color(v, rgb);
    SDL_RenderRect(v->renderer, &b);
}

static void text(View *v, Face *face, float x, float top, float size, unsigned rgb, const char *s)
{
    float scale = size / 54.0f, baseline = top + size * 0.8f;
    SDL_SetTextureColorMod(face->texture, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb);
    for (; *s; ++s) {
        unsigned ch = (unsigned char)*s;
        const stbtt_bakedchar *g = &face->glyph[(ch >= 32 && ch < 127 ? ch : '?') - 32];
        SDL_FRect src = {g->x0, g->y0, g->x1 - g->x0, g->y1 - g->y0};
        SDL_FRect dst = {x + g->xoff * scale, baseline + g->yoff * scale, src.w * scale, src.h * scale};
        if (src.w > 0) SDL_RenderTexture(v->renderer, face->texture, &src, &dst);
        x += g->xadvance * scale;
    }
}

static void body(View *v, float x, float y, int size, unsigned rgb, const char *s)
{
    text(v, &v->body, x, y, (float)size, rgb, s);
}

static void title(View *v, float x, float y, int size, const char *s)
{
    text(v, &v->title, x, y, (float)size, ink, s);
}

static void tile(View *v, int f, SDL_FRect b, float scale)
{
    SDL_RenderTextureTiled(v->renderer, v->fabric[f], NULL, scale, &b);
}

SDL_FRect quilt_box(const Quilt *q, SDL_FRect mat)
{
    float side = floorf(fminf((mat.w - 60) / q->cols, (mat.h - 60) / q->rows));
    float w = side * q->cols, h = side * q->rows;
    return (SDL_FRect){floorf(mat.x + (mat.w - w) / 2), floorf(mat.y + (mat.h - h) / 2), w, h};
}

static void mat_background(View *v, SDL_FRect mat)
{
    box(v, mat, 0xe9e6d9);
    color(v, 0xc8c6b7);
    for (float y = mat.y + 9; y < mat.y + mat.h; y += 14)
        for (float x = mat.x + 9; x < mat.x + mat.w; x += 14)
            SDL_RenderPoint(v->renderer, x, y);
    border(v, mat, rule);
}

enum { TEXTURED = 1, LETTERS = 2, FOCUSED = 4 };

static void fabric_piece(View *v, SDL_FRect cell, const Patch *patch, int turn, int fabric, unsigned style)
{
    SDL_Vertex mesh[MAX_VERTICES];
    SDL_FPoint edge[MAX_VERTICES + 1], center = {0}, min = {cell.w, cell.h}, max = {0};
    int triangles[(MAX_VERTICES - 2) * 3];
    int n = patch->vertices;
    SDL_FColor fill = {1, 1, 1, 1};
    if (!(style & TEXTURED)) {
        unsigned rgb = fabric_rgb[fabric];
        fill = (SDL_FColor){((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, 1};
    }
    for (int i = 0; i < n; ++i) {
        Point p = turn_point(patch->point[i], turn);
        SDL_FPoint uv = {p.x / (float)BLOCK_UNITS, p.y / (float)BLOCK_UNITS};
        float x = uv.x * cell.w, y = uv.y * cell.h;
        edge[i] = (SDL_FPoint){cell.x + x, cell.y + y};
        mesh[i] = (SDL_Vertex){edge[i], fill, uv};
        center.x += edge[i].x / n;
        center.y += edge[i].y / n;
        min.x = SDL_min(min.x, x); max.x = SDL_max(max.x, x);
        min.y = SDL_min(min.y, y); max.y = SDL_max(max.y, y);
    }
    for (int i = 0; i < n - 2; ++i) {
        triangles[3 * i] = 0;
        triangles[3 * i + 1] = i + 1;
        triangles[3 * i + 2] = i + 2;
    }
    SDL_RenderGeometry(v->renderer, style & TEXTURED ? v->fabric[fabric] : NULL, mesh, n,
                       triangles, (n - 2) * 3);
    edge[n] = edge[0];
    color(v, 0x847c68);
    SDL_RenderLines(v->renderer, edge, n + 1);
    float room = SDL_min(max.x - min.x, max.y - min.y);
    if ((style & LETTERS) && room >= 12) {
        int size = SDL_min(20, (int)(room * 0.62f));
        char letter[2] = {(char)('A' + fabric), 0};
        body(v, center.x - size * 0.22f, center.y - size * 0.49f, size,
             fabric == 3 ? paper : 0x242b23, letter);
    }
    if (style & FOCUSED) {
        for (int i = 0; i <= n; ++i) {
            edge[i].x += (center.x - edge[i].x) * 0.12f;
            edge[i].y += (center.y - edge[i].y) * 0.12f;
        }
        color(v, paper);
        SDL_RenderLines(v->renderer, edge, n + 1);
    }
}

static void quilt_pieces(View *v, const Quilt *q, SDL_FRect b, const App *focus, unsigned style)
{
    float side = b.w / q->cols;
    for (int r = 0; r < q->rows; ++r) {
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            SDL_FRect cell = {b.x + c * side, b.y + r * side, side, side};
            int picked = focus && focus->cursor && r == focus->row && c == focus->col ? focus->patch : -1;
            const Pattern *pattern = block_pattern(q, block);
            if (block->turn || !(style & TEXTURED)) {
                for (int p = 0; p < pattern->count; ++p)
                    fabric_piece(v, cell, &pattern->patches[p], block->turn, block->fabric[p],
                                 style | (p == picked ? FOCUSED : 0));
                continue;
            }
            int fabric = block->fabric[0];
            tile(v, fabric, cell, style & LETTERS ? 0.65f : 1.0f);
            color(v, 0x847c68);
            SDL_RenderLine(v->renderer, cell.x, cell.y + side - 1, cell.x + side - 1, cell.y + side - 1);
            SDL_RenderLine(v->renderer, cell.x + side - 1, cell.y, cell.x + side - 1, cell.y + side - 1);
            if (style & LETTERS) {
                char letter[2] = {(char)('A' + fabric), 0};
                body(v, cell.x + side / 2 - 5, cell.y + side / 2 - 10, 20, fabric == 3 ? paper : 0x242b23, letter);
            }
            if (picked == 0) {
                border(v, (SDL_FRect){cell.x + 2, cell.y + 2, side - 4, side - 4}, paper);
                border(v, (SDL_FRect){cell.x + 3, cell.y + 3, side - 6, side - 6}, ink);
            }
        }
    }
}

static void board(View *v, const Quilt *q, SDL_FRect mat, const App *focus, unsigned style)
{
    mat_background(v, mat);
    SDL_FRect b = quilt_box(q, mat);
    box(v, (SDL_FRect){b.x - 5, b.y - 4, b.w + 14, b.h + 14}, 0xc6c3b3);
    box(v, (SDL_FRect){b.x - 7, b.y - 7, b.w + 14, b.h + 14}, 0xfffcf0);
    quilt_pieces(v, q, b, focus, style);
}

static void stepper(View *v, Control minus, const char *label, int value, int low, int high)
{
    SDL_FRect left = control_box[minus], right = control_box[minus + 1];
    char number[8];
    snprintf(number, sizeof number, "%d", value);
    body(v, 755, left.y + 5, 21, ink, label);
    border(v, left, rule); border(v, right, rule);
    body(v, left.x + 12, left.y + 2, 25, value == low ? rule : green, "-");
    body(v, right.x + 10, right.y + 3, 23, value == high ? rule : green, "+");
    body(v, left.x + 56, left.y + 4, 21, ink, number);
}

static void design(App *a)
{
    View *v = a->view;
    const Quilt *q = &a->history.steps[a->history.at];
    char line[96];
    title(v, 36, 85, 57, patterns[q->pattern].name);
    snprintf(line, sizeof line, "%d x %d %s   /   %d x %d in finished",
             q->cols, q->rows, q->pattern ? "blocks" : "patches", q->cols * q->inches, q->rows * q->inches);
    body(v, 38, 139, 20, muted, line);
    board(v, q, design_mat, a, TEXTURED);
    body(v, 37, 641, 18, muted, "Choose a fabric, then click a piece to place it.");
    title(v, 754, 105, 38, "Fabric basket");
    for (int f = 0; f < FABRICS; ++f) {
        SDL_FRect b = control_box[SWATCH_A + f];
        bool picked = f == a->fabric;
        if (picked) { box(v, b, 0xe8ecdf); border(v, b, green); }
        tile(v, f, (SDL_FRect){b.x + 8, b.y + 7, 34, 30}, 0.8f);
        char code[4] = {(char)('A' + f), 0};
        body(v, b.x + 54, b.y + 12, 18, muted, code);
        body(v, b.x + 88, b.y + 9, 23, ink, fabric_name[f]);
        code[0] = (char)('1' + f);
        body(v, b.x + b.w - 23, b.y + 13, 16, muted, code);
    }
    box(v, (SDL_FRect){754, 415, 286, 1}, rule);
    title(v, 754, 425, 37, "Quilt size");
    stepper(v, COL_LESS, "Columns", q->cols, MIN_SIDE, MAX_SIDE);
    stepper(v, ROW_LESS, "Rows", q->rows, MIN_SIDE, MAX_SIDE);
    stepper(v, PATCH_LESS, q->pattern ? "Block (in)" : "Square (in)", q->inches, MIN_PATCH, MAX_PATCH);
    body(v, 755, 623, 17, muted, "Size is measured after sewing.");
}

static void sheet(App *a)
{
    View *v = a->view;
    const Quilt *q = &a->history.steps[a->history.at];
    int counts[FABRICS], w = q->cols * q->inches, h = q->rows * q->inches;
    char line[128];
    count_pieces(q, counts);
    title(v, 36, 85, 57, "Cutting sheet");
    snprintf(line, sizeof line, "Finished top   %d x %d in    /    %.1f x %.1f cm", w, h, w * 2.54, h * 2.54);
    body(v, 38, 142, 21, muted, line);
    box(v, (SDL_FRect){36, 190, 634, 86}, 0xe8ecdf);
    if (q->pattern) {
        int total = 0;
        for (int f = 0; f < FABRICS; ++f) total += counts[f];
        snprintf(line, sizeof line, "Cut %d pieces for %d blocks", total, q->rows * q->cols);
    } else {
        snprintf(line, sizeof line, "Cut %d squares at %d.5 x %d.5 in", q->rows * q->cols, q->inches, q->inches);
    }
    title(v, 56, 199, 36, line);
    body(v, 57, 246, 18, muted, q->pattern ? "Templates include a 1/4 in seam on every edge." :
                                           "Includes a 1/4 in seam on all four edges.");
    body(v, 48, 307, 17, muted, "FABRIC");
    body(v, 587, 307, 17, muted, "PIECES");
    for (int f = 0; f < FABRICS; ++f) {
        float y = 341 + f * 55;
        box(v, (SDL_FRect){36, y - 5, 634, 1}, rule);
        tile(v, f, (SDL_FRect){48, y + 3, 39, 32}, 0.8f);
        snprintf(line, sizeof line, "%c    %s", 'A' + f, fabric_name[f]);
        body(v, 105, y + 6, 24, ink, line);
        snprintf(line, sizeof line, "%d", counts[f]);
        body(v, 609, y + 6, 24, green, line);
    }
    title(v, 721, 100, 39, "Row map");
    body(v, 723, 147, 19, muted, q->pattern ? "Take the sheet for cut templates." : "Sew one row at a time, top to bottom.");
    board(v, q, (SDL_FRect){717, 192, 323, 328}, NULL, TEXTURED | LETTERS);
    SDL_FRect b = control_box[SHARE];
    box(v, b, green);
#ifdef __EMSCRIPTEN__
    body(v, b.x + 45, b.y + 11, 24, paper, "Download cutting sheet");
#else
    body(v, b.x + 65, b.y + 11, 24, paper, "Copy cutting sheet");
#endif
}

static void choice(View *v, SDL_FRect b, const char *label, bool picked, bool enabled)
{
    if (picked) box(v, b, 0xe8ecdf);
    border(v, b, picked ? green : rule);
    body(v, b.x + 13, b.y + 7, 21, enabled ? ink : muted, label);
}

static void library(App *a)
{
    static const char *const filters[FILTER_COUNT] = {"All", "Traditional", "Modern"};
    View *v = a->view;
    int ids[PATTERN_COUNT];
    int count = catalog_ids(a->filter, ids);
    int start = a->chosen / CARDS_PER_PAGE * CARDS_PER_PAGE;
    int current = ids[a->chosen];
    char line[80];
    title(v, 36, 85, 57, "Pattern library");
    snprintf(line, sizeof line, "%d designs / %d settings each", count, SETTING_COUNT);
    body(v, 420, 153, 20, muted, line);
    for (int i = 0; i < FILTER_COUNT; ++i)
        choice(v, control_box[FILTER_ALL + i], filters[i], i == (int)a->filter, true);

    for (int i = 0; i < CARDS_PER_PAGE && start + i < count; ++i) {
        SDL_FRect card = pattern_card(i);
        const Pattern *pattern = &patterns[ids[start + i]];
        bool picked = start + i == a->chosen;
        box(v, card, picked ? 0xe8ecdf : 0xfffcf5);
        border(v, card, picked ? green : rule);
        SDL_FRect thumb = {card.x + 36, card.y + 12, 122, 122};
        for (int p = 0; p < pattern->count; ++p)
            fabric_piece(v, thumb, &pattern->patches[p], 1, pattern->patches[p].fabric, 0);
        body(v, card.x + 12, card.y + 143, 22, ink, pattern->name);
    }
    choice(v, control_box[PREV_PAGE], "Previous", false, start > 0);
    choice(v, control_box[NEXT_PAGE], "Next", false, start + CARDS_PER_PAGE < count);
    snprintf(line, sizeof line, "%d / %d", start / CARDS_PER_PAGE + 1,
             (count + CARDS_PER_PAGE - 1) / CARDS_PER_PAGE);
    body(v, 155, 628, 19, muted, line);
    body(v, 312, 628, 19, muted, "Undo restores your previous layout.");
    box(v, (SDL_FRect){672, 99, 1, 558}, rule);

    Quilt preview = a->history.steps[a->history.at];
    arrange(&preview, current, a->setting);
    title(v, 705, 98, 43, patterns[current].name);
    snprintf(line, sizeof line, "%d x %d blocks   /   %d x %d in finished", preview.cols, preview.rows,
             preview.cols * preview.inches, preview.rows * preview.inches);
    body(v, 706, 142, 18, muted, line);
    board(v, &preview, (SDL_FRect){705, 169, 335, 220}, NULL, 0);
    title(v, 705, 399, 34, "Quilt setting");
    for (int i = 0; i < SETTING_COUNT; ++i)
        choice(v, setting_box(i), setting_name[i], i == (int)a->setting, true);
    SDL_FRect apply = control_box[APPLY];
    box(v, apply, green);
    body(v, apply.x + 91, apply.y + 8, 24, paper, "Use this pattern");
#ifdef __EMSCRIPTEN__
    snprintf(line, sizeof line, "Pattern library. %s, %s. %s.", patterns[current].name, setting_name[a->setting],
             filters[a->filter]);
    browser_describe_view(line);
#endif
}

void draw_app(App *a)
{
    View *v = a->view;
    color(v, paper);
    SDL_RenderClear(v->renderer);
    box(v, (SDL_FRect){0, 71, WALL_WIDTH, 1}, rule);
    for (int i = 0; i < 4; ++i)
        tile(v, (int[]){1, 4, 2, 3}[i], (SDL_FRect){36 + (i % 2) * 17, 22 + (i / 2) * 17, 16, 16}, 0.6f);
    title(v, 84, 12, 45, "Patchwork");
    body(v, 395, 26, 23, a->screen == DESIGN ? green : muted, "Design wall");
    body(v, 521, 26, 23, a->screen == SHEET ? green : muted, "Cutting sheet");
    body(v, 679, 26, 23, a->screen == LIBRARY ? green : muted, "Patterns");
    SDL_FRect tab = control_box[a->screen];
    box(v, (SDL_FRect){tab.x + 10, 68, tab.w - 20, 3}, green);
    border(v, control_box[UNDO], rule); border(v, control_box[REDO], rule);
    body(v, 872, 25, 22, a->history.at ? ink : muted, "Undo");
    body(v, 969, 25, 22, a->history.at < a->history.last ? ink : muted, "Redo");
    if (a->screen == SHEET) sheet(a);
    else if (a->screen == LIBRARY) library(a);
    else design(a);
    box(v, (SDL_FRect){0, 675, WALL_WIDTH, 45}, 0xf0eee5);
    box(v, (SDL_FRect){0, 675, WALL_WIDTH, 1}, rule);
    body(v, 36, 687, 18, green, a->status);
    const Quilt *q = &a->history.steps[a->history.at];
#ifdef __EMSCRIPTEN__
    if (a->screen != LIBRARY) {
        char description[80];
        snprintf(description, sizeof description, "%s. %s.", a->screen == SHEET ? "Cutting sheet" : "Design wall",
                 patterns[q->pattern].name);
        browser_describe_view(description);
    }
#endif
    const char *keys = a->screen == SHEET ? "Tab  Design wall      Enter  Take sheet" :
                       a->screen == LIBRARY ? "Arrows  Design      [ ]  Setting      Enter  Apply" :
                       q->pattern ? "1-5  Fabric    Arrows  Block    [ ]  Piece    Space  Place" :
                                    "1-5  Fabric      Arrows  Move      Space  Place";
    body(v, 616, 687, 17, muted, keys);
    SDL_RenderPresent(v->renderer);
}
