#include "pdf.h"
#include "whole.h"
#include "stb_truetype.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

enum { FIRST_PAGE = 8, MAX_PAGES = 384, PAPER_WIDTH = 595, PAPER_HEIGHT = 842, MARGIN = 44 };
enum { FIRST_GLYPH = 32, LAST_GLYPH = 0x17f, BODY_SIZE = 10, LINE_HEIGHT = 14 };

typedef struct { SDL_IOStream *file; bool ok; } Writer;
typedef struct {
    stbtt_fontinfo info;
    double scale;
    int glyph[LAST_GLYPH - FIRST_GLYPH + 1];
    int advance[LAST_GLYPH - FIRST_GLYPH + 1];
} Font;

const void *pdf_data(SDL_IOStream *file)
{
    return SDL_GetPointerProperty(SDL_GetIOProperties(file), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);
}

static void put(Writer *out, const char *format, ...)
{
    if (!out->ok) return;
    va_list args;
    va_start(args, format);
    out->ok = SDL_IOvprintf(out->file, format, args) != 0;
    va_end(args);
}

static void bytes(Writer *out, const void *data, size_t size)
{
    if (out->ok) out->ok = SDL_WriteIO(out->file, data, size) == size;
}

static void object(Writer *out, Sint64 *offset, int id)
{
    offset[id] = SDL_TellIO(out->file);
    put(out, "%d 0 obj\n", id);
}

static void stream_object(Writer *out, Sint64 *offset, int id, const void *data, size_t size, bool font)
{
    object(out, offset, id);
    put(out, "<< /Length %zu", size);
    if (font) put(out, " /Length1 %zu", size);
    put(out, " >>\nstream\n");
    bytes(out, data, size);
    put(out, "\nendstream\nendobj\n");
}

static int code_index(Uint32 code)
{
    return (int)(code >= FIRST_GLYPH && code <= LAST_GLYPH ? code : '?') - FIRST_GLYPH;
}

static bool font_metrics(Font *font, const unsigned char *data)
{
    if (!stbtt_InitFont(&font->info, data, 0)) return false;
    font->scale = stbtt_ScaleForMappingEmToPixels(&font->info, 1000);
    for (int code = FIRST_GLYPH; code <= LAST_GLYPH; ++code) {
        int index = code_index((Uint32)code), advance;
        int glyph = stbtt_FindGlyphIndex(&font->info, code);
        font->glyph[index] = glyph;
        stbtt_GetGlyphHMetrics(&font->info, glyph, &advance, NULL);
        font->advance[index] = (int)lround(advance * font->scale);
    }
    return true;
}

static void unicode_map(Writer *out, Sint64 *offset, const Font *font)
{
    Writer map = {SDL_IOFromDynamicMem(), true};
    if (!map.file) { out->ok = false; return; }
    put(&map, "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n"
              "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
              "/CMapName /PatchworkUnicode def\n/CMapType 2 def\n"
              "1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n");
    for (int start = FIRST_GLYPH; start <= LAST_GLYPH; start += 64) {
        int end = SDL_min(start + 64, LAST_GLYPH + 1), count = 0;
        for (int code = start; code < end; ++code) count += font->glyph[code_index((Uint32)code)] != 0;
        if (!count) continue;
        put(&map, "%d beginbfchar\n", count);
        for (int code = start; code < end; ++code) {
            int glyph = font->glyph[code_index((Uint32)code)];
            if (glyph) put(&map, "<%04X> <%04X>\n", (unsigned)glyph, (unsigned)code);
        }
        put(&map, "endbfchar\n");
    }
    put(&map, "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n");
    if (map.ok) stream_object(out, offset, 7, pdf_data(map.file), (size_t)SDL_GetIOSize(map.file), false);
    else out->ok = false;
    SDL_CloseIO(map.file);
}

static void font_objects(Writer *out, Sint64 *offset, Font *font, const void *data, size_t length)
{
    object(out, offset, 3);
    put(out, "<< /Type /Font /Subtype /Type0 /BaseFont /DMSans-Regular /Encoding /Identity-H"
             " /DescendantFonts [4 0 R] /ToUnicode 7 0 R >>\nendobj\n");
    object(out, offset, 4);
    put(out, "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /DMSans-Regular"
             " /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >>"
             " /FontDescriptor 5 0 R /CIDToGIDMap /Identity /W [");
    for (int id = 1; id < font->info.numGlyphs; ++id)
        for (int c = FIRST_GLYPH; c <= LAST_GLYPH; ++c) {
            int index = code_index((Uint32)c);
            if (font->glyph[index] == id) {
                put(out, "%d [%d] ", id, font->advance[index]);
                break;
            }
        }
    put(out, "] >>\nendobj\n");
    object(out, offset, 5);
    int left, bottom, right, top, ascent, descent;
    stbtt_GetFontBoundingBox(&font->info, &left, &bottom, &right, &top);
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, NULL);
    put(out, "<< /Type /FontDescriptor /FontName /DMSans-Regular /Flags 32"
             " /FontBBox [%.0f %.0f %.0f %.0f] /ItalicAngle 0 /Ascent %.0f"
             " /Descent %.0f /CapHeight %.0f /StemV 80 /FontFile2 6 0 R >>\nendobj\n",
             left * font->scale, bottom * font->scale, right * font->scale, top * font->scale,
             ascent * font->scale, descent * font->scale, 700.0);
    stream_object(out, offset, 6, data, length, true);
    unicode_map(out, offset, font);
}

static void text_at(Writer *page, const Font *font, double x, double y, int size, const char *p, const char *end)
{
    put(page, "BT /F1 %d Tf 1 0 0 1 %.3f %.3f Tm <", size, x, y);
    size_t length = (size_t)(end - p);
    for (Uint32 code; (code = SDL_StepUTF8(&p, &length)) != 0;)
        put(page, "%04X", (unsigned)font->glyph[code_index(code)]);
    put(page, "> Tj ET\n");
}

static const char *line_end(const Font *font, const char *text)
{
    const char *last_space = NULL, *p = text;
    int width = 0;
    const int limit = 1000 * (PAPER_WIDTH - 2 * MARGIN) / BODY_SIZE;
    while (*p && *p != '\n') {
        const char *next = p;
        Uint32 code = SDL_StepUTF8(&next, NULL);
        width += font->advance[code_index(code)];
        if (width > limit && p != text) return last_space ? last_space : p;
        if (code == ' ' && p != text) last_space = p;
        p = next;
    }
    return p;
}

typedef struct {
    Writer *page;
    const Quilt *q;
    const Font *font;
    double left, top, step;
    Layer layer;
} Diagram;

static void path(Diagram *d, const XY *p, int n, bool closed)
{
    for (int i = 0; i < n; ++i)
        put(d->page, "%.3f %.3f %c ", d->left + p[i].x * d->step, d->top - p[i].y * d->step, i ? 'l' : 'm');
    if (closed) put(d->page, "h ");
}

static void map_piece(const Outline *s, void *data)
{
    Diagram *d = data;
    unsigned rgb = d->q->color[s->fabric];
    put(d->page, "%.3f %.3f %.3f rg ", (rgb >> 16) / 255.0, ((rgb >> 8) & 255) / 255.0, (rgb & 255) / 255.0);
    path(d, s->p, s->n, true);
    if (s->bridge) {
        put(d->page, "f ");
        path(d, s->p, s->n / 2, false);
        path(d, s->p + s->n / 2, s->n / 2, false);
        put(d->page, "S\n");
    } else
        put(d->page, "B\n");
}

static void map_quilt(Writer *page, const Quilt *q)
{
    double step = fmin((PAPER_WIDTH - 2.0 * MARGIN) / q->cols, 274.0 / q->rows);
    double left = (PAPER_WIDTH - step * q->cols) / 2, top = 753;
    put(page, "q 0.4 w 0.42 0.4 0.36 RG\n");
    Diagram d = {page,q,NULL,left,top,step,FOUNDATION};
    if (q->foundation) visit_layer(q,FOUNDATION,map_piece,&d);
    else for (int r = 0; r < q->rows; ++r)
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            BlockBounds b = block_bounds(q, r, c);
            const Pattern *pattern = block_pattern(block);
            for (int p = 0; p < pattern->count; ++p) {
                const Patch *patch = &pattern->patches[p];
                unsigned rgb = q->color[block->fabric[p]];
                put(page, "%.3f %.3f %.3f rg ", (rgb >> 16) / 255.0, ((rgb >> 8) & 255) / 255.0, (rgb & 255) / 255.0);
                for (int v = 0; v < patch->vertices; ++v) {
                    Point point = turn_point(patch->point[v], block->turn);
                    put(page, "%.3f %.3f %c ", left + step * (b.x + b.w * point.x / BLOCK_UNITS),
                        top - step * (b.y + b.h * point.y / BLOCK_UNITS), v == 0 ? 'm' : 'l');
                }
                put(page, "h B\n");
            }
        }
    visit_layer(q,APPLIQUE,map_piece,&d);
    put(page, "Q\n");
}

static void finish_page(Writer *out, Sint64 *offset, Writer *page, int number)
{
    int id = FIRST_PAGE + number * 2;
    object(out, offset, id);
    put(out,
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d]"
        " /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>\nendobj\n",
        PAPER_WIDTH, PAPER_HEIGHT, id + 1);
    if (page->ok)
        stream_object(out, offset, id + 1, pdf_data(page->file), (size_t)SDL_GetIOSize(page->file), false);
    else
        out->ok = false;
    SDL_CloseIO(page->file);
}

enum { FRAME_W = PAPER_WIDTH - 2 * MARGIN, FRAME_H = 654, FRAME_TOP = 742 };
typedef struct {
    double x0, y0, x1, y1;
} Bounds;

static void include(Bounds *b, XY p)
{
    b->x0 = fmin(b->x0, p.x);
    b->x1 = fmax(b->x1, p.x);
    b->y0 = fmin(b->y0, p.y);
    b->y1 = fmax(b->y1, p.y);
}

typedef struct {
    Diagram d;
    Bounds bounds;
    bool measuring;
} Trace;

static void trace_piece(const Outline *s, void *data)
{
    if (s->bridge) return;
    Trace *t = data;
    Diagram *d = &t->d;
    XY cut[MAX_CUT_POINTS];
    int ncut = 0;
    Bounds bounds = {s->p[0].x, s->p[0].y, s->p[0].x, s->p[0].y};
    if (d->layer == FOUNDATION) ncut = cut_outline(s, (double)SEAM_ALLOWANCE / d->q->size, cut);
    for (int i = 0; i < s->n; ++i) include(&bounds, s->p[i]);
    for (int i = 0; i < ncut; ++i) include(&bounds, cut[i]);
    if (t->measuring) {
        include(&t->bounds, (XY){bounds.x0, bounds.y0});
        include(&t->bounds, (XY){bounds.x1, bounds.y1});
        return;
    }
    if (bounds.x1 < t->bounds.x0 || bounds.y1 < t->bounds.y0 || bounds.x0 > t->bounds.x1 || bounds.y0 > t->bounds.y1)
        return;
    Writer *page = d->page;
    if (d->layer == FOUNDATION) {
        put(page, "0.48 G 0.38 w [3 2] 0 d ");
        path(d, cut, ncut, true);
        put(page, "S ");
    }
    put(page, "0 G 0.65 w [] 0 d ");
    path(d, s->p, s->n, true);
    put(page, "S\n");
    XY pos = outline_label(s);
    if (pos.x < t->bounds.x0 || pos.x >= t->bounds.x1 || pos.y < t->bounds.y0 || pos.y >= t->bounds.y1) return;
    char label[16];
    snprintf(label, sizeof label, "%c%d/%c", d->layer == FOUNDATION ? 'T' : 'A', s->id + 1, 'A' + s->fabric);
    text_at(page, d->font, d->left + pos.x * d->step - 6, d->top - pos.y * d->step, 7, label, label + strlen(label));
}

static int trace_pages(Writer *out, Sint64 *offset, const Font *font, const Quilt *q, Preferences prefs, int count)
{
    const double step = 72.0 * q->size / INCH;
    Trace t = {.d = {NULL, q, font, 0, 0, step, FOUNDATION}, .bounds = {0, 0, q->cols, q->rows}, .measuring = true};
    visit_layer(q, FOUNDATION, trace_piece, &t);
    Bounds extent = t.bounds;
    int nx = (int)ceil((extent.x1 - extent.x0) * step / FRAME_W);
    int ny = (int)ceil((extent.y1 - extent.y0) * step / FRAME_H);
    t.measuring = false;
    for (int layer = 0; layer < LAYER_COUNT; ++layer) {
        int design = layer == FOUNDATION ? q->foundation : q->applique;
        if (!design) continue;
        t.d.layer = (Layer)layer;
        for (int y = 0; y < ny; ++y)
            for (int x = 0; x < nx; ++x) {
                if (!out->ok || count >= MAX_PAGES) {
                    out->ok = false;
                    return count;
                }
                Writer page = {SDL_IOFromDynamicMem(), true};
                if (!page.file) {
                    out->ok = false;
                    return count;
                }
                t.d.page = &page;
                t.bounds = (Bounds){extent.x0 + x * FRAME_W / step, extent.y0 + y * FRAME_H / step,
                                    extent.x0 + (x + 1) * FRAME_W / step, extent.y0 + (y + 1) * FRAME_H / step};
                t.d.left = MARGIN - t.bounds.x0 * step;
                t.d.top = FRAME_TOP + t.bounds.y0 * step;
                char title[150];
                snprintf(title, sizeof title, tr(prefs.language, TRACE_TILE), tr(prefs.language, whole_name[design]),
                         x + 1, y + 1, nx, ny);
                text_at(&page, font, MARGIN, 801, 14, title, title + strlen(title));
                const char *help = tr(prefs.language, PRINT_SCALE);
                text_at(&page, font, MARGIN, 779, 10, help, help + strlen(help));
                help = tr(prefs.language, layer == FOUNDATION ? TRACE_PIECED : TRACE_RAW);
                text_at(&page, font, MARGIN, 761, 9, help, help + strlen(help));
                put(&page, "q %d %d %d %d re W n\n", MARGIN, FRAME_TOP - FRAME_H, FRAME_W, FRAME_H);
                if (layer == APPLIQUE)
                    put(&page, "0.5 G 0.5 w [1 2] 0 d %.3f %.3f %.3f %.3f re S\n", t.d.left, t.d.top - q->rows * step,
                        q->cols * step, q->rows * step);
                visit_layer(q, (Layer)layer, trace_piece, &t);
                put(&page, "Q 0 G 0.35 w %d %d %d %d re S\n", MARGIN, FRAME_TOP - FRAME_H, FRAME_W, FRAME_H);
                double ruler = prefs.unit == INCHES ? 72 : 5 * 72 / 2.54;
                put(&page, "0.8 w %d 57 m %.3f 57 l S %d 53 m %d 61 l S %.3f 53 m %.3f 61 l S\n", MARGIN,
                    MARGIN + ruler, MARGIN, MARGIN, MARGIN + ruler, MARGIN + ruler);
                snprintf(title, sizeof title, "%s  %d %s", tr(prefs.language, SCALE_CHECK),
                         prefs.unit == INCHES ? 1 : 5, unit_label(prefs));
                text_at(&page, font, MARGIN + ruler + 10, 53, 10, title, title + strlen(title));
                finish_page(out, offset, &page, count++);
            }
    }
    return count;
}

static int pages(Writer *out, Sint64 *offset, const Font *font, const Quilt *q, const char *text)
{
    int count = 0;
    while (out->ok && *text && count < MAX_PAGES) {
        Writer page = {SDL_IOFromDynamicMem(), true};
        if (!page.file) { out->ok = false; break; }
        int y = PAPER_HEIGHT - MARGIN;
        if (!count) {
            const char *title = strchr(text, '\n');
            text_at(&page, font, MARGIN, y, 17, text, title);
            map_quilt(&page, q);
            text = title + 1;
            y = 456;
        }
        while (page.ok && *text && y >= MARGIN) {
            const char *end = line_end(font, text);
            if (end != text) text_at(&page, font, MARGIN, y, BODY_SIZE, text, end);
            text = end + (*end == '\n' || *end == ' ');
            y -= LINE_HEIGHT;
        }
        finish_page(out,offset,&page,count++);
    }
    if (*text) out->ok = false;
    return count;
}

SDL_IOStream *make_pdf(const Quilt *q, Preferences prefs)
{
    char *path = NULL;
    const char *base = SDL_GetBasePath();
    if (!base || SDL_asprintf(&path, "%sassets/DM-Sans.ttf", base) < 0) return NULL;
    size_t length;
    unsigned char *data = SDL_LoadFile(path, &length);
    SDL_free(path);
    char *text = SDL_malloc(SHEET_BYTES);
    Font font;
    Writer out = {NULL, true};
    if (data && text && font_metrics(&font, data) && cutting_sheet(q, prefs, text)) out.file = SDL_IOFromDynamicMem();
    if (out.file) {
        Sint64 offset[FIRST_PAGE + MAX_PAGES * 2] = {0};
        bytes(&out, "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n", 15);
        object(&out, offset, 1);
        put(&out, "<< /Type /Catalog /Pages 2 0 R /ViewerPreferences << /PrintScaling /None >> /Lang (%s) >>\nendobj\n", language_tag[prefs.language]);
        font_objects(&out, offset, &font, data, length);
        int count = pages(&out, offset, &font, q, text);
        count = trace_pages(&out,offset,&font,q,prefs,count);
        object(&out, offset, 2);
        put(&out, "<< /Type /Pages /Count %d /Kids [", count);
        for (int p = 0; p < count; ++p) put(&out, "%d 0 R ", FIRST_PAGE + p * 2);
        put(&out, "] >>\nendobj\n");
        Sint64 xref = SDL_TellIO(out.file);
        int objects = FIRST_PAGE + count * 2;
        put(&out, "xref\n0 %d\n0000000000 65535 f \n", objects);
        for (int id = 1; id < objects; ++id) put(&out, "%010lld 00000 n \n", (long long)offset[id]);
        put(&out, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%lld\n%%%%EOF\n", objects, (long long)xref);
        if (!out.ok || !count) {
            SDL_CloseIO(out.file);
            out.file = NULL;
        }
    }
    SDL_free(text);
    SDL_free(data);
    return out.file;
}
