#include "app.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb_truetype.h"

#ifdef __EMSCRIPTEN__
extern void browser_describe_view(const char *description, const char *keys, const char *lang);
#endif

enum { FIRST_GLYPH = 32, LAST_GLYPH = 0x17f, GLYPHS = LAST_GLYPH - FIRST_GLYPH + 1 };
typedef struct {
    SDL_Texture *texture;
    stbtt_packedchar glyph[GLYPHS];
} Face;

struct View {
    SDL_Renderer *renderer;
    Face body, title;
    SDL_Texture *fabric[MAX_FABRICS];
    uint32_t color[MAX_FABRICS];
};

static const unsigned ink = 0x263e35, muted = 0x56665b, paper = 0xfaf8f2,
    rule = 0xd6d4c6, green = 0x325545;
const SDL_FRect design_mat = {36, 165, 670, 466};
const SDL_FRect control_box[CONTROL_COUNT] = {
    [DESIGN] = {376, 12, 125, 48}, [SHEET] = {509, 12, 139, 48}, [LIBRARY] = {656, 12, 133, 48},
    [COLORS] = {788, 12, 64, 48}, [LANGUAGE_MENU] = {230, 19, 127, 36},
    [UNDO] = {853, 19, 89, 36}, [REDO] = {950, 19, 89, 36},
    [SWATCH_A] = {754, 157, 286, 44}, [SWATCH_B] = {754, 205, 286, 44},
    [SWATCH_C] = {754, 253, 286, 44}, [SWATCH_D] = {754, 301, 286, 44},
    [SWATCH_E] = {754, 349, 286, 44},
    [COL_LESS] = {914, 475, 35, 34}, [COL_MORE] = {1005, 475, 35, 34},
    [ROW_LESS] = {914, 523, 35, 34}, [ROW_MORE] = {1005, 523, 35, 34},
    [PATCH_LESS] = {914, 571, 35, 34}, [PATCH_MORE] = {1005, 571, 35, 34},
    [SET_SIZE] = {951, 571, 52, 34}, [UNIT_IN] = {754, 620, 34, 33}, [UNIT_CM] = {788, 620, 42, 33},
    [SET_SEAMS] = {839, 620, 201, 33},
    [ROTATE_BLOCK] = {436, 636, 128, 30}, [FINISH_PLACING] = {575, 636, 131, 30},
    [SHARE] = {753, 560, 287, 44}, [EXPORT_PDF] = {753, 613, 287, 44},
    [FILTER_ALL] = {36, 150, 95, 37}, [FILTER_TRADITIONAL] = {143, 150, 122, 37},
    [FILTER_MODERN] = {277, 150, 106, 37},
    [FILTER_WHOLE] = {395, 150, 143, 37},
    [EDIT_LAYER] = {519, 636, 187, 30}, [REMOVE_OVERLAY] = {876, 551, 164, 35},
    [RESTORE_BLOCKS] = {705, 551, 164, 35},
    [PREV_PAGE] = {36, 620, 101, 37}, [NEXT_PAGE] = {203, 620, 85, 37},
    [APPLY] = {705, 615, 164, 42}, [BRUSH] = {876, 615, 164, 42},
    [ORIGINAL_COLORS] = {754, 450, 286, 40},
    [LANGUAGE_EN] = {230, 59, 127, 36}, [LANGUAGE_DE] = {230, 95, 127, 36},
    [LANGUAGE_PL] = {230, 131, 127, 36},
    [EDIT_APPLY] = {617, 484, 153, 41}, [EDIT_CANCEL] = {446, 484, 153, 41}
};

SDL_FRect pattern_card(int slot)
{
    return (SDL_FRect){36 + (slot % CARDS_PER_ROW) * 207, 201 + (slot / CARDS_PER_ROW) * 191, 194, 178};
}

SDL_FRect setting_box(int setting)
{
    return (SDL_FRect){705 + (setting % 2) * 171, 440 + (setting / 2) * 42, 164, 36};
}

SDL_FRect palette_card(int slot)
{
    return (SDL_FRect){36 + (slot % 2) * 319, 164 + (slot / 2) * 84, 303, 72};
}

SDL_FRect slider_box(int channel)
{
    return (SDL_FRect){355, 342 + channel * 35, 370, 21};
}

SDL_FRect fabric_nav(Control id, Control screen)
{
    float x = screen == SHEET ? 48 : 754, y = screen == SHEET ? 619 : 122;
    if (id == FABRIC_PREV) return (SDL_FRect){x, y, 30, 28};
    if (id == FABRIC_NEXT) return (SDL_FRect){x + 100, y, 30, 28};
    return (SDL_FRect){x + 143, y, 143, 28};
}

static bool load_face(View *v, Face *face, const char *base, const char *name)
{
    char *path = NULL;
    if (!base || SDL_asprintf(&path, "%sassets/%s", base, name) < 0) return false;
    unsigned char *font = SDL_LoadFile(path, NULL);
    SDL_free(path);
    if (!font) return false;

    const int width = 1024, height = 512;
    unsigned char *alpha = SDL_malloc((size_t)width * height);
    SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
    stbtt_pack_context pack;
    bool packed = false;
    if (alpha && surface && stbtt_PackBegin(&pack, alpha, width, height, 0, 1, NULL)) {
        packed = stbtt_PackFontRange(&pack, font, 0, 54, FIRST_GLYPH, GLYPHS, face->glyph) != 0;
        stbtt_PackEnd(&pack);
    }
    if (packed) {
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

static SDL_Texture *fabric_tile(SDL_Renderer *renderer, int f, unsigned rgb)
{
    /* WebGL 1 requires power-of-two textures for repeating fabric patterns. */
    SDL_Surface *s = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return NULL;
    for (int y = 0; y < s->h; ++y) {
        Uint32 *pixels = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
        for (int x = 0; x < s->w; ++x) {
            int weave = (x % 3 == 0) * 4 - (y % 3 == 0) * 5 + (x * 13 + y * 7) % 5 - 2;
            int offset = motif(f % STARTER_FABRICS, x, y) + weave;
            unsigned color = 0xff000000;
            for (int shift = 0; shift <= 16; shift += 8) {
                int channel = (int)((rgb >> shift) & 255);
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
    for (int f = 0; ok && f < STARTER_FABRICS; ++f) {
        v->color[f] = original_palette[f];
        ok = (v->fabric[f] = fabric_tile(renderer, f, v->color[f])) != NULL;
    }
    if (ok) return v;
    close_view(v);
    return NULL;
}

void close_view(View *v)
{
    if (!v) return;
    SDL_DestroyTexture(v->body.texture);
    SDL_DestroyTexture(v->title.texture);
    for (int f = 0; f < MAX_FABRICS; ++f) SDL_DestroyTexture(v->fabric[f]);
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

static const stbtt_packedchar *font_glyph(const Face *face, Uint32 code)
{
    return &face->glyph[(code >= FIRST_GLYPH && code <= LAST_GLYPH ? code : '?') - FIRST_GLYPH];
}

static void text(View *v, Face *face, float x, float top, float size, unsigned rgb, const char *s)
{
    float scale = size / 54.0f, baseline = top + size * 0.8f;
    SDL_SetTextureColorMod(face->texture, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb);
    for (Uint32 code; (code = SDL_StepUTF8(&s, NULL)) != 0;) {
        const stbtt_packedchar *g = font_glyph(face, code);
        SDL_FRect src = {g->x0, g->y0, g->x1 - g->x0, g->y1 - g->y0};
        SDL_FRect dst = {x + g->xoff * scale, baseline + g->yoff * scale, src.w * scale, src.h * scale};
        if (src.w > 0) SDL_RenderTexture(v->renderer, face->texture, &src, &dst);
        x += g->xadvance * scale;
    }
}

static void fitted(View *v, Face *face, SDL_FRect b, int size, unsigned rgb, const char *s, bool center)
{
    float width = 0;
    const char *p = s;
    for (Uint32 code; (code = SDL_StepUTF8(&p, NULL)) != 0;)
        width += font_glyph(face, code)->xadvance * size / 54.0f;
    float scale = width > b.w ? b.w / width : 1;
    float x = center ? b.x + (b.w - scale * width) / 2 : b.x;
    text(v, face, x, b.y, size * scale, rgb, s);
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
    if (v->fabric[f]) SDL_RenderTextureTiled(v->renderer, v->fabric[f], NULL, scale, &b);
    else box(v, b, v->color[f]);
}

static unsigned fabric_ink(const View *v, int f)
{
    unsigned rgb = v->color[f];
    unsigned light = 54 * (rgb >> 16) + 183 * ((rgb >> 8) & 255) + 19 * (rgb & 255);
    return light < 34304 ? paper : 0x242b23;
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
    SDL_Texture *texture = style & TEXTURED ? v->fabric[fabric] : NULL;
    if (!texture) {
        unsigned rgb = v->color[fabric];
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
    SDL_RenderGeometry(v->renderer, texture, mesh, n, triangles, (n - 2) * 3);
    edge[n] = edge[0];
    color(v, 0x847c68);
    SDL_RenderLines(v->renderer, edge, n + 1);
    float room = SDL_min(max.x - min.x, max.y - min.y);
    if ((style & LETTERS) && room >= 12) {
        int size = SDL_min(20, (int)(room * 0.62f));
        char letter[2] = {(char)('A' + fabric), 0};
        body(v, center.x - size * 0.22f, center.y - size * 0.49f, size, fabric_ink(v, fabric), letter);
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

typedef struct {
    View *v;
    SDL_FRect box;
    double step;
    unsigned style;
    int selected;
} ClothDrawing;

static void draw_outline(const Outline *s, void *data)
{
    ClothDrawing *d = data;
    View *v = d->v;
    SDL_Vertex mesh[OUTLINE_POINTS];
    SDL_FPoint edge[OUTLINE_POINTS + 1];
    int indices[3 * (OUTLINE_POINTS - 2)];
    unsigned rgb = v->color[s->fabric];
    SDL_FColor fill = {((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, 1};
    for (int i = 0; i < s->n; ++i) {
        edge[i] = (SDL_FPoint){(float)(d->box.x + s->p[i].x * d->step), (float)(d->box.y + s->p[i].y * d->step)};
        mesh[i] = (SDL_Vertex){edge[i], fill, {0, 0}};
    }
    int count = outline_mesh(s, indices);
    if (count) SDL_RenderGeometry(v->renderer, NULL, mesh, s->n, indices, count);
    edge[s->n] = edge[0];
    color(v, s->id == d->selected ? 0x263e35 : 0x887e69);
    if (s->bridge) {
        SDL_RenderLines(v->renderer, edge, s->n / 2);
        SDL_RenderLines(v->renderer, edge + s->n / 2, s->n / 2);
        return;
    }
    SDL_RenderLines(v->renderer, edge, s->n + 1);
    if ((d->style & LETTERS) || s->id == d->selected) {
        XY p = outline_label(s);
        float x = (float)(d->box.x + d->step * p.x), y = (float)(d->box.y + d->step * p.y);
        if (s->id == d->selected) {
            box(v, (SDL_FRect){x - 10, y - 10, 20, 20}, paper);
            border(v, (SDL_FRect){x - 10, y - 10, 20, 20}, ink);
        }
        char code[] = {(char)('A' + s->fabric), 0};
        body(v, x - 4, y - 7, 15, s->id == d->selected ? ink : fabric_ink(v, s->fabric), code);
    }
}

static void quilt_pieces(View *v, const Quilt *q, SDL_FRect b, const App *focus, unsigned style)
{
    float side = b.w / q->cols;
    ClothDrawing d = {v, b, side, style, -1};
    if (focus && focus->cursor && focus->selected_layer == FOUNDATION) d.selected = focus->whole_piece;
    if (q->foundation)
        visit_layer(q, FOUNDATION, draw_outline, &d);
    else
        for (int r = 0; r < q->rows; ++r) {
            for (int c = 0; c < q->cols; ++c) {
                const Block *block = &q->cell[r][c];
                BlockBounds shape = block_bounds(q, r, c);
                SDL_FRect cell = {b.x + (float)shape.x * side, b.y + (float)shape.y * side, (float)shape.w * side,
                                  (float)shape.h * side};
                int picked = focus && focus->cursor && focus->selected_layer == FOUNDATION && r == focus->row &&
                                     c == focus->col
                                 ? focus->patch
                                 : -1;
                const Pattern *pattern = block_pattern(block);
                if (block->turn || !(style & TEXTURED)) {
                    for (int p = 0; p < pattern->count; ++p)
                        fabric_piece(v, cell, &pattern->patches[p], block->turn, block->fabric[p],
                                     style | (p == picked ? FOCUSED : 0));
                    continue;
                }
                int fabric = block->fabric[0];
                tile(v, fabric, cell, style & LETTERS ? 0.65f : 1.0f);
                color(v, 0x847c68);
                SDL_RenderLine(v->renderer, cell.x, cell.y + cell.h - 1, cell.x + cell.w - 1, cell.y + cell.h - 1);
                SDL_RenderLine(v->renderer, cell.x + cell.w - 1, cell.y, cell.x + cell.w - 1, cell.y + cell.h - 1);
                if (style & LETTERS) {
                    char letter[2] = {(char)('A' + fabric), 0};
                    body(v, cell.x + cell.w / 2 - 5, cell.y + cell.h / 2 - 10, 20, fabric_ink(v, fabric), letter);
                }
                if (picked == 0) {
                    border(v, (SDL_FRect){cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4}, paper);
                    border(v, (SDL_FRect){cell.x + 3, cell.y + 3, cell.w - 6, cell.h - 6}, ink);
                }
                if (focus && focus->brush && picked >= 0) {
                    border(v, (SDL_FRect){cell.x + 1, cell.y + 1, cell.w - 2, cell.h - 2}, green);
                }
            }
        }
    d.selected = focus && focus->cursor && focus->selected_layer == APPLIQUE ? focus->whole_piece : -1;
    visit_layer(q,APPLIQUE,draw_outline,&d);
}

static void board(View *v, const Quilt *q, SDL_FRect mat, const App *focus, unsigned style)
{
    mat_background(v, mat);
    SDL_FRect b = quilt_box(q, mat);
    box(v, (SDL_FRect){b.x - 5, b.y - 4, b.w + 14, b.h + 14}, 0xc6c3b3);
    box(v, (SDL_FRect){b.x - 7, b.y - 7, b.w + 14, b.h + 14}, 0xfffcf0);
    quilt_pieces(v, q, b, focus, style);
}

static void button_label(View *v, SDL_FRect b, const char *name, int size, unsigned rgb)
{
    fitted(v, &v->body, (SDL_FRect){b.x + 7, b.y + (b.h - size) / 2, b.w - 14, b.h}, size, rgb, name, true);
}

static void choice(View *v, SDL_FRect b, const char *label, bool picked, bool enabled)
{
    if (picked) box(v, b, 0xe8ecdf);
    border(v, b, picked ? green : rule);
    button_label(v, b, label, 21, enabled ? ink : muted);
}

static void action(View *v, Control control, const char *label, bool primary)
{
    SDL_FRect b = control_box[control];
    box(v, b, primary ? green : paper);
    border(v, b, green);
    button_label(v, b, label, 22, primary ? paper : green);
}

static void stepper(View *v, Control minus, const char *label, const char *number, bool low, bool high)
{
    SDL_FRect left = control_box[minus], right = control_box[minus + 1];
    fitted(v, &v->body, (SDL_FRect){755, left.y + 5, 151, 34}, 21, ink, label, false);
    border(v, left, rule); border(v, right, rule);
    button_label(v, left, "-", 24, low ? rule : green);
    button_label(v, right, "+", 24, high ? rule : green);
    SDL_FRect value = {left.x + left.w + 2, left.y, right.x - (left.x + left.w) - 4, left.h};
    if (minus == PATCH_LESS) border(v, value, rule);
    button_label(v, value, number, 21, ink);
}

static void basket_nav(App *a, const Quilt *q)
{
    View *v = a->view;
    int start = a->fabric_page * STARTER_FABRICS, end = SDL_min(q->fabrics, start + STARTER_FABRICS);
    SDL_FRect prev = fabric_nav(FABRIC_PREV, a->screen), next = fabric_nav(FABRIC_NEXT, a->screen);
    choice(v, prev, "<", false, start > 0);
    choice(v, next, ">", false, end < q->fabrics);
    char range[] = {(char)('A' + start), ' ', '-', ' ', (char)('A' + end - 1), 0};
    if (end == start + 1) range[1] = 0;
    button_label(v, (SDL_FRect){prev.x + 30, prev.y, 70, prev.h}, range, 17, ink);
    if (a->screen != SHEET)
        choice(v, fabric_nav(ADD_SWATCH, a->screen),
               tr(a->prefs.language, q->fabrics < MAX_FABRICS ? ADD_FABRIC : FABRIC_LIMIT), false,
               q->fabrics < MAX_FABRICS);
}

static void fabric_swatches(App *a, const Quilt *q, bool edit_mode)
{
    View *v = a->view;
    Language lang = a->prefs.language;
    basket_nav(a, q);
    for (int slot = 0; slot < STARTER_FABRICS; ++slot) {
        int f = a->fabric_page * STARTER_FABRICS + slot;
        if (f >= q->fabrics) break;
        SDL_FRect b = control_box[SWATCH_A + slot];
        bool picked = f == a->fabric && !a->brush;
        if (picked) { box(v, b, 0xe8ecdf); border(v, b, green); }
        else if (edit_mode) border(v, b, rule);
        tile(v, f, (SDL_FRect){b.x + 8, b.y + 7, 34, 30}, 0.8f);
        char code[8] = {(char)('A' + f), 0};
        body(v, b.x + 54, b.y + 12, 18, muted, code);
        const char *name = NULL;
        if (edit_mode || f >= STARTER_FABRICS || q->color[f] != original_palette[f]) {
            snprintf(code, sizeof code, "#%06X", (unsigned)q->color[f]);
            name = code;
        } else name = fabric_name[f][lang];
        fitted(v, &v->body, (SDL_FRect){b.x + 81, b.y + 9, b.w - 111, 36}, 22, ink, name, false);
        char key[2] = {(char)('1' + slot), 0};
        body(v, b.x + b.w - 23, b.y + 13, 16, muted, key);
    }
}

static void design(App *a, const Quilt *q)
{
    View *v = a->view;
    Preferences prefs = a->prefs;
    Language lang = prefs.language;
    char line[256], size[2 * NUMBER_BYTES + 16], value[NUMBER_BYTES];
    fitted(v, &v->title, (SDL_FRect){36, 85, a->brush ? 595 : 666, 53}, 57, ink, quilt_name(q, lang), false);
    dimensions(size, prefs, q->cols * q->size, q->rows * q->size, 3, true);
    snprintf(line, sizeof line, tr(lang, q->pattern || mixed_quilt(q) ? GRID_BLOCKS : GRID_PATCHES),
             q->cols, q->rows, size);
    if (q->foundation || q->applique) {
        const char *base = q->foundation    ? tr(lang, whole_name[q->foundation])
                           : mixed_quilt(q) ? tr(lang, MIXED_NAME)
                                            : patterns[q->pattern].name[lang];
        if (q->applique)
            snprintf(line, sizeof line, "%s   /   %s", base, size);
        else
            snprintf(line, sizeof line, tr(lang, WHOLE_DIMENSIONS), size);
    }
    body(v, 38, 139, 20, muted, line);
    board(v, q, design_mat, a, TEXTURED);
    if (a->brush) {
        snprintf(line, sizeof line, tr(lang, PLACE_HINT), patterns[a->brush].name[lang]);
        fitted(v, &v->body, (SDL_FRect){37, 641, 383, 24}, 18, green, line, false);
        const Pattern *brush = &patterns[a->brush];
        SDL_FRect thumb = {651, 96, 53, 53};
        for (int i = 0; i < brush->count; ++i)
            fabric_piece(v, thumb, &brush->patches[i], a->turn, brush->patches[i].fabric, 0);
        choice(v, control_box[ROTATE_BLOCK], tr(lang, ROTATE_NAME), false, true);
        choice(v, control_box[FINISH_PLACING], tr(lang, FINISH_NAME), false, true);
    } else {
        body(v, 37, 641, 18, muted, tr(lang, CHOOSE_FABRIC));
        if (q->applique) choice(v,control_box[EDIT_LAYER],tr(lang,a->background_only ? PICK_ALL : PICK_TOP),false,true);
    }
    title(v, 754, 80, 38, tr(lang, FABRIC_BASKET));
    fabric_swatches(a, q, false);
    box(v, (SDL_FRect){754, 415, 286, 1}, rule);
    title(v, 754, 425, 37, tr(lang, QUILT_SIZE));
    snprintf(value, sizeof value, "%d", q->cols);
    stepper(v, COL_LESS, tr(lang, COLUMNS_NAME), value, q->cols == MIN_SIDE, q->cols == MAX_SIDE);
    snprintf(value, sizeof value, "%d", q->rows);
    stepper(v, ROW_LESS, tr(lang, ROWS_NAME), value, q->rows == MIN_SIDE, q->rows == MAX_SIDE);
    decimal(value, lang, q->size / (double)unit_scale(prefs.unit), 3, true);
    stepper(v, PATCH_LESS, tr(lang, EDIT_SIZE), value, q->size == MIN_SIZE, q->size == MAX_SIZE);
    choice(v, control_box[UNIT_IN], "in", prefs.unit == INCHES, true);
    choice(v, control_box[UNIT_CM], "cm", prefs.unit == CENTIMETRES, true);
    snprintf(line, sizeof line, "%s / %s", tr(lang, SEAMS_NAME), seam_name[q->seams][lang]);
    if (q->foundation) button_label(v,control_box[SET_SEAMS],tr(lang,whole_name[q->foundation]),19,muted);
    else choice(v, control_box[SET_SEAMS], line, false, true);
}

static void sheet(App *a, const Quilt *q)
{
    View *v = a->view;
    Language lang = a->prefs.language;
    int counts[MAX_FABRICS];
    char line[200], size[2 * NUMBER_BYTES + 16], other_size[2 * NUMBER_BYTES + 16];
    count_pieces(q, counts);
    title(v, 36, 85, 57, tr(lang, SHEET_NAME));
    double width = q->cols * q->size, height = q->rows * q->size;
    dimensions(size, a->prefs, width, height, 3, true);
    Preferences other = {lang, a->prefs.unit == INCHES ? CENTIMETRES : INCHES};
    dimensions(other_size, other, width, height, 3, true);
    snprintf(line, sizeof line, "%s   /   %s", size, other_size);
    char top[240];
    snprintf(top, sizeof top, tr(lang, TOP_SIZE), line);
    fitted(v, &v->body, (SDL_FRect){38, 142, 630, 30}, 21, muted, top, false);
    box(v, (SDL_FRect){36, 190, 634, 86}, 0xe8ecdf);
    bool whole = q->foundation || q->applique;
    bool pieced = whole || q->pattern || mixed_quilt(q) || q->seams;
    if (pieced) {
        int total = 0;
        for (int f = 0; f < q->fabrics; ++f) total += counts[f];
        if (whole) snprintf(line,sizeof line,tr(lang,WHOLE_TOTAL),total);
        else snprintf(line, sizeof line, tr(lang, CUT_PIECES), total, q->rows * q->cols);
    } else {
        dimensions(size, a->prefs, q->size + 2 * SEAM_ALLOWANCE, q->size + 2 * SEAM_ALLOWANCE, 3, true);
        snprintf(line, sizeof line, tr(lang, CUT_SQUARES), q->rows * q->cols, size);
    }
    fitted(v, &v->title, (SDL_FRect){56, 202, 592, 40}, 36, ink, line, false);
    snprintf(line, sizeof line, tr(lang, SEAM_SHORT), seam_measure(a->prefs));
    body(v, 57, 246, 18, muted, whole ? tr(lang,TRACE_HELP) : line);
    body(v, 48, 307, 17, muted, tr(lang, SHEET_FABRIC));
    fitted(v, &v->body, (SDL_FRect){553, 307, 100, 22}, 17, muted, tr(lang, SHEET_PIECES), true);
    if (q->fabrics > STARTER_FABRICS) basket_nav(a, q);
    for (int slot = 0; slot < STARTER_FABRICS; ++slot) {
        int f = a->fabric_page * STARTER_FABRICS + slot;
        if (f >= q->fabrics) break;
        float y = 341 + slot * 55;
        box(v, (SDL_FRect){36, y - 5, 634, 1}, rule);
        tile(v, f, (SDL_FRect){48, y + 3, 39, 32}, 0.8f);
        if (f < STARTER_FABRICS && q->color[f] == original_palette[f])
            snprintf(line, sizeof line, "%c    %s", 'A' + f, fabric_name[f][lang]);
        else
            snprintf(line, sizeof line, "%c    #%06X", 'A' + f, (unsigned)q->color[f]);
        body(v, 105, y + 6, 24, ink, line);
        snprintf(line, sizeof line, "%d", counts[f]);
        fitted(v, &v->body, (SDL_FRect){553, y + 6, 100, 30}, 24, green, line, true);
    }
    title(v, 721, 100, 39, tr(lang, whole ? WHOLE_NAME : ROW_MAP));
    fitted(v, &v->body, (SDL_FRect){723, 147, 324, 30}, 19, muted, tr(lang, pieced ? MAP_USAGE : ROW_USAGE), false);
    board(v, q, (SDL_FRect){717, 192, 323, 328}, NULL, TEXTURED | LETTERS);
#ifdef __EMSCRIPTEN__
    action(v, SHARE, tr(lang, DOWNLOAD_TEXT), false);
    action(v, EXPORT_PDF, tr(lang, DOWNLOAD_PDF), true);
#else
    action(v, SHARE, tr(lang, COPY_TEXT), false);
    action(v, EXPORT_PDF, tr(lang, SAVE_PDF), true);
#endif
}

static void pager(View *v, Language lang, int page, int pages)
{
    choice(v, control_box[PREV_PAGE], tr(lang, PREVIOUS), false, page > 0);
    choice(v, control_box[NEXT_PAGE], tr(lang, NEXT), false, page + 1 < pages);
    char label[16];
    snprintf(label, sizeof label, "%d / %d", page + 1, pages);
    body(v, 147, 628, 19, muted, label);
}

static void whole_library(App *a, const Quilt *q)
{
    View *v = a->view;
    Language lang = a->prefs.language;
    for (int id = HEX_FIELD; id < WHOLE_COUNT; ++id) {
        SDL_FRect card = pattern_card(id - HEX_FIELD);
        bool picked = (int)a->whole_choice == id;
        box(v, card, picked ? 0xe8ecdf : 0xfffcf5);
        border(v, card, picked ? green : rule);
        Quilt sample = {.rows = 3, .cols = 3, .foundation = WHOLECLOTH};
        if (id >= VINE)
            sample.applique = (uint8_t)id;
        else
            sample.foundation = (uint8_t)id;
        quilt_pieces(v, &sample, (SDL_FRect){card.x + 34, card.y + 11, 126, 126}, NULL, 0);
        fitted(v, &v->body, (SDL_FRect){card.x + 12, card.y + 144, card.w - 24, 28}, 22, ink,
               tr(lang, whole_name[id]), false);
    }
    fitted(v, &v->body, (SDL_FRect){36, 627, 617, 25}, 19, muted, tr(lang, WHOLE_GUIDE), false);
    box(v, (SDL_FRect){672, 99, 1, 558}, rule);
    int id = a->whole_choice;
    bool applique = id >= VINE;
    Quilt preview = *q;
    set_whole_design(&preview, applique ? APPLIQUE : FOUNDATION, (WholeDesign)id);
    fitted(v, &v->title, (SDL_FRect){705, 98, 335, 49}, 43, ink, tr(lang, whole_name[id]), false);
    body(v, 706, 147, 19, muted, tr(lang, applique ? APPLIQUE_NAME : FOUNDATION_NAME));
    board(v, &preview, (SDL_FRect){705, 180, 335, 316}, NULL, 0);
    fitted(v, &v->body, (SDL_FRect){706, 512, 334, 27}, 19, ink, tr(lang, whole_help[id]), false);
    if (q->foundation) choice(v, control_box[RESTORE_BLOCKS], tr(lang, RESTORE_TOP), false, true);
    if (q->applique) choice(v, control_box[REMOVE_OVERLAY], tr(lang, LIFT_APPLIQUE), false, true);
    action(v, APPLY, tr(lang, applique ? ON_QUILT : USE_TOP), true);
    if (applique) action(v, BRUSH, tr(lang, ON_CLOTH), false);
}

static void library(App *a, const Quilt *q)
{
    static const Text filters[FILTER_COUNT] = {ALL_NAME, TRADITIONAL_NAME, MODERN_NAME};
    View *v = a->view;
    Language lang = a->prefs.language;
    int ids[PATTERN_COUNT];
    int count = catalog_ids(a->filter, ids);
    int start = a->chosen / CARDS_PER_PAGE * CARDS_PER_PAGE;
    int current = ids[a->chosen];
    char line[256], size[2 * NUMBER_BYTES + 16];
    title(v, 36, 85, 57, tr(lang, LIBRARY_NAME));
    snprintf(line, sizeof line, tr(lang, DESIGN_COUNT), count, SETTING_COUNT);
    if (!a->whole_library) fitted(v, &v->body, (SDL_FRect){546, 159, 117, 24}, 17, muted, line, true);
    for (int i = 0; i < FILTER_COUNT; ++i)
        choice(v, control_box[FILTER_ALL + i], tr(lang, filters[i]), !a->whole_library && i == (int)a->filter, true);
    choice(v,control_box[FILTER_WHOLE],tr(lang,WHOLE_NAME),a->whole_library,true);
    if (a->whole_library) { whole_library(a,q); return; }

    for (int i = 0; i < CARDS_PER_PAGE && start + i < count; ++i) {
        SDL_FRect card = pattern_card(i);
        const Pattern *pattern = &patterns[ids[start + i]];
        bool picked = start + i == a->chosen;
        box(v, card, picked ? 0xe8ecdf : 0xfffcf5);
        border(v, card, picked ? green : rule);
        SDL_FRect thumb = {card.x + 36, card.y + 12, 122, 122};
        for (int p = 0; p < pattern->count; ++p)
            fabric_piece(v, thumb, &pattern->patches[p], 1, pattern->patches[p].fabric, 0);
        fitted(v, &v->body, (SDL_FRect){card.x + 12, card.y + 143, card.w - 24, 30}, 22, ink, pattern->name[lang], false);
    }
    pager(v, lang, start / CARDS_PER_PAGE, (count + CARDS_PER_PAGE - 1) / CARDS_PER_PAGE);
    fitted(v, &v->body, (SDL_FRect){312, 628, 339, 23}, 19, muted, tr(lang, PATTERN_UNDO), false);
    box(v, (SDL_FRect){672, 99, 1, 558}, rule);

    Quilt preview = *q;
    arrange(&preview, current, a->setting);
    fitted(v, &v->title, (SDL_FRect){705, 98, 335, 48}, 43, ink, patterns[current].name[lang], false);
    dimensions(size, a->prefs, preview.cols * preview.size, preview.rows * preview.size, 3, true);
    snprintf(line, sizeof line, tr(lang, GRID_BLOCKS), preview.cols, preview.rows, size);
    fitted(v, &v->body, (SDL_FRect){706, 142, 334, 22}, 18, muted, line, false);
    board(v, &preview, (SDL_FRect){705, 169, 335, 220}, NULL, 0);
    title(v, 705, 399, 34, tr(lang, SETTING_NAME));
    for (int i = 0; i < SETTING_COUNT; ++i)
        choice(v, setting_box(i), setting_name[i][lang], i == (int)a->setting, true);
    action(v, APPLY, tr(lang, APPLY_PATTERN), false);
    action(v, BRUSH, tr(lang, PLACE_BLOCK), true);
}

static void colors(App *a, const Quilt *q)
{
    View *v = a->view;
    Language lang = a->prefs.language;
    char name[PALETTE_NAME_BYTES];
    title(v, 36, 85, 57, tr(lang, PALETTES_NAME));
    snprintf(name, sizeof name, "%d", PALETTE_COUNT);
    body(v, 602, 112, 21, muted, name);
    for (int slot = 0; slot < PALETTES_PER_PAGE; ++slot) {
        int id = a->palette_page * PALETTES_PER_PAGE + slot;
        SDL_FRect card = palette_card(slot);
        uint32_t tones[STARTER_FABRICS];
        palette_colors(id, tones);
        palette_name(id, lang, name);
        bool picked = memcmp(tones, q->color, sizeof tones) == 0;
        box(v, card, picked ? 0xe8ecdf : 0xfffcf5);
        border(v, card, picked ? green : rule);
        fitted(v, &v->body, (SDL_FRect){card.x + 11, card.y + 7, card.w - 22, 30}, 21, ink, name, false);
        float w = (card.w - 22) / STARTER_FABRICS;
        for (int f = 0; f < STARTER_FABRICS; ++f)
            box(v, (SDL_FRect){card.x + 11 + f * w, card.y + 37, w, 24}, tones[f]);
    }
    pager(v, lang, a->palette_page, PALETTE_COUNT / PALETTES_PER_PAGE);
    fitted(v, &v->body, (SDL_FRect){309, 628, 348, 23}, 18, muted, tr(lang, PALETTES_HELP), false);
    title(v, 754, 80, 38, tr(lang, COLOR_LIBRARY));
    fabric_swatches(a, q, true);
    fitted(v, &v->body, (SDL_FRect){754, 413, 286, 30}, 19, muted, tr(lang, CUSTOM_HELP), false);
    action(v, ORIGINAL_COLORS, tr(lang, ORIGINAL_PALETTE), false);
}

static void language_menu(App *a)
{
    View *v = a->view;
    SDL_FRect button = control_box[LANGUAGE_MENU];
    box(v, button, paper);
    border(v, button, a->language_menu ? green : rule);
    body(v, button.x + 12, button.y + 8, 20, ink, language_name[a->prefs.language]);
    color(v, green);
    SDL_RenderLine(v->renderer, button.x + 105, button.y + 15, button.x + 110, button.y + 20);
    SDL_RenderLine(v->renderer, button.x + 110, button.y + 20, button.x + 115, button.y + 15);
    if (!a->language_menu) return;
    for (int i = 0; i < LANGUAGE_COUNT; ++i) {
        SDL_FRect b = control_box[LANGUAGE_EN + i];
        box(v, b, i == (int)a->prefs.language ? 0xe8ecdf : paper);
        border(v, b, i == (int)a->prefs.language ? green : rule);
        body(v, b.x + 12, b.y + 8, 20, ink, language_name[i]);
        char number[2] = {(char)('1' + i), 0};
        body(v, b.x + 109, b.y + 9, 17, muted, number);
    }
}

static void sliders(View *v, unsigned rgb)
{
    for (int channel = 0; channel < 3; ++channel) {
        SDL_FRect b = slider_box(channel);
        int shift = 16 - 8 * channel;
        unsigned base = rgb & ~(255u << shift);
        for (int step = 0; step < 32; ++step) {
            unsigned tint = base | ((unsigned)(step * 255 / 31) << shift);
            float w = b.w / 32;
            box(v, (SDL_FRect){b.x + step * w, b.y, w + 0.5f, b.h}, tint);
        }
        border(v, b, rule);
        float x = b.x + ((rgb >> shift) & 255) * b.w / 255;
        border(v, (SDL_FRect){x - 3, b.y - 2, 6, b.h + 4}, paper);
        border(v, (SDL_FRect){x - 4, b.y - 3, 8, b.h + 6}, ink);
        char code[2] = {"RGB"[channel], 0};
        body(v, b.x - 26, b.y, 19, muted, code);
    }
}

static void edit_popup(App *a, const Quilt *q)
{
    View *v = a->view;
    Language lang = a->prefs.language;
    char line[144], low[NUMBER_BYTES], high[NUMBER_BYTES];
    SDL_FRect panel = {287, 177, 505, 373}, field = {327, 271, 272, 43};
    if (a->editor == EDIT_SIZE_VALUE) {
        panel.y = 233;
        panel.h = 317;
        field.y = 325;
    }
    box(v, (SDL_FRect){panel.x + 5, panel.y + 6, panel.w, panel.h}, 0xbcb9aa);
    box(v, panel, paper);
    border(v, panel, green);
    if (a->editor == EDIT_FABRIC) {
        snprintf(line, sizeof line, tr(lang, a->adding ? ADD_FABRIC_TITLE : EDIT_COLOR), 'A' + (a->adding ? q->fabrics : a->fabric));
        title(v, 327, panel.y + 23, 42, line);
        body(v, 327, field.y - 25, 17, muted, tr(lang, CUSTOM_COLOR));
        int parsed = parse_color(a->input);
        unsigned rgb = parsed < 0 ? a->initial_color : (unsigned)parsed;
        box(v, (SDL_FRect){625, field.y, 102, field.h}, rgb);
        border(v, (SDL_FRect){625, field.y, 102, field.h}, rule);
        sliders(v, rgb);
        snprintf(line, sizeof line, "%s", tr(lang, HEX_HELP));
    } else {
        title(v, 327, panel.y + 24, 42, tr(lang, EDIT_SIZE));
        body(v, 622, field.y + 8, 24, muted, unit_label(a->prefs));
        decimal(low, lang, MIN_SIZE / (double)unit_scale(a->prefs.unit), 4, true);
        decimal(high, lang, MAX_SIZE / (double)unit_scale(a->prefs.unit), 4, true);
        snprintf(line, sizeof line, tr(lang, SIZE_HELP), low, high);
    }
    box(v, field, a->replace_input ? 0xe8ecdf : 0xfffcf5);
    border(v, field, a->invalid ? 0xb86761 : green);
    fitted(v, &v->body, (SDL_FRect){field.x + 11, field.y + 7, field.w - 22, 33}, 25, ink, a->input, false);
    fitted(v, &v->body, (SDL_FRect){327, 453, 433, 24}, 18, a->invalid ? 0x8e453d : muted,
           a->invalid ? tr(lang, INVALID_VALUE) : line, false);
    action(v, EDIT_CANCEL, tr(lang, CANCEL_NAME), false);
    action(v, EDIT_APPLY, tr(lang, APPLY_NAME), true);
}

#ifdef __EMSCRIPTEN__
static void describe(const App *a, const Quilt *q)
{
    Language lang = a->prefs.language;
    Text keys = CANVAS_KEYS;
    char line[256];
    if (a->editor) {
        keys = CANVAS_EDIT;
        snprintf(line, sizeof line, "%s. %s", tr(lang, a->editor == EDIT_FABRIC ? CUSTOM_COLOR : EDIT_SIZE), a->input);
    } else if (a->language_menu) {
        keys = CANVAS_LANGUAGE;
        snprintf(line, sizeof line, "%s. %s.", tr(lang, LANGUAGE_NAME), language_name[lang]);
    } else if (a->screen == LIBRARY && a->whole_library) {
        keys = CANVAS_WHOLE;
        snprintf(line, sizeof line, "%s. %s. %s.", tr(lang, WHOLE_NAME), tr(lang, whole_name[a->whole_choice]),
                 tr(lang, whole_help[a->whole_choice]));
    } else if (a->screen == LIBRARY) {
        static const Text filters[FILTER_COUNT] = {ALL_NAME, TRADITIONAL_NAME, MODERN_NAME};
        int ids[PATTERN_COUNT];
        catalog_ids(a->filter, ids);
        keys = CANVAS_PATTERNS;
        snprintf(line, sizeof line, "%s. %s, %s. %s.", tr(lang, LIBRARY_NAME), patterns[ids[a->chosen]].name[lang],
                 setting_name[a->setting][lang], tr(lang, filters[a->filter]));
    } else if (a->screen == COLORS) {
        keys = CANVAS_COLORS;
        snprintf(line, sizeof line, "%s. %s %d / %d.", tr(lang, COLORS_NAME), tr(lang, PALETTES_NAME),
                 a->palette_page + 1, PALETTE_COUNT / PALETTES_PER_PAGE);
    } else {
        snprintf(line, sizeof line, "%s. %s.", tr(lang, a->screen == SHEET ? SHEET_NAME : DESIGN_NAME), quilt_name(q, lang));
    }
    if (!a->editor && !a->language_menu && (a->screen == DESIGN || a->screen == COLORS || a->screen == SHEET)) {
        size_t used = strlen(line);
        snprintf(line + used, sizeof line - used, " %s %c. %c-%c / %d.", tr(lang, FABRIC_BASKET), 'A' + a->fabric,
                 'A' + a->fabric_page * STARTER_FABRICS,
                 'A' + SDL_min(q->fabrics, (a->fabric_page + 1) * STARTER_FABRICS) - 1, q->fabrics);
    }
    browser_describe_view(line, tr(lang, keys), language_tag[lang]);
}
#endif

void draw_app(App *a)
{
    View *v = a->view;
    const Quilt *q = &a->history.steps[a->history.at];
    Language lang = a->prefs.language;
    for (int f = 0; f < q->fabrics; ++f)
        if (!v->fabric[f] || v->color[f] != q->color[f]) {
            SDL_DestroyTexture(v->fabric[f]);
            v->color[f] = q->color[f];
            v->fabric[f] = fabric_tile(v->renderer, f, v->color[f]);
        }
    color(v, paper);
    SDL_RenderClear(v->renderer);
    box(v, (SDL_FRect){0, 71, WALL_WIDTH, 1}, rule);
    for (int i = 0; i < 4; ++i)
        box(v, (SDL_FRect){36 + (i % 2) * 17, 22 + (i / 2) * 17, 16, 16}, original_palette[(int[]){1, 4, 2, 3}[i]]);
    title(v, 84, 12, 45, "Patchwork");
    static const Text tabs[] = {DESIGN_NAME, SHEET_NAME, PATTERNS_NAME, COLORS_NAME};
    for (Control screen = DESIGN; screen <= COLORS; ++screen)
        button_label(v, control_box[screen], tr(lang, tabs[screen]), screen == COLORS ? 19 : 23,
                     a->screen == screen ? green : muted);
    SDL_FRect tab = control_box[a->screen];
    box(v, (SDL_FRect){tab.x + 8, 68, tab.w - 16, 3}, green);
    choice(v, control_box[UNDO], tr(lang, UNDO_NAME), false, a->history.at > 0);
    choice(v, control_box[REDO], tr(lang, REDO_NAME), false, a->history.at < a->history.last);
    if (a->screen == SHEET) sheet(a, q);
    else if (a->screen == LIBRARY) library(a, q);
    else if (a->screen == COLORS) colors(a, q);
    else design(a, q);
    box(v, (SDL_FRect){0, 675, WALL_WIDTH, 45}, 0xf0eee5);
    box(v, (SDL_FRect){0, 675, WALL_WIDTH, 1}, rule);
    fitted(v, &v->body, (SDL_FRect){36, 687, 557, 23}, 18, green, tr(lang, a->status), false);
    Text keys = a->language_menu ? LANGUAGE_KEYS : a->screen == SHEET ? SHEET_KEYS :
                a->screen == LIBRARY ? (a->whole_library ? (a->whole_choice < VINE ? WHOLE_TOP_KEYS : WHOLE_KEYS) : LIBRARY_KEYS) : a->screen == COLORS ? COLOR_KEYS : DESIGN_KEYS;
    fitted(v, &v->body, (SDL_FRect){600, 687, 440, 23}, 17, muted, tr(lang, keys), false);
    language_menu(a);
    if (a->editor) edit_popup(a, q);
#ifdef __EMSCRIPTEN__
    describe(a, q);
#endif
    SDL_RenderPresent(v->renderer);
}
