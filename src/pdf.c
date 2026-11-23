#include "pdf.h"
#include "stb_truetype.h"
#include <math.h>
#include <stdarg.h>
#include <string.h>

enum { FIRST_PAGE = 8, MAX_PAGES = 256, PAPER_WIDTH = 595, PAPER_HEIGHT = 842, MARGIN = 44 };
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

static void line(Writer *page, const Font *font, int y, int size, const char *p, const char *end)
{
    put(page, "BT /F1 %d Tf 1 0 0 1 %d %d Tm <", size, MARGIN, y);
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

static void map_quilt(Writer *page, const Quilt *q)
{
    double step = fmin((PAPER_WIDTH - 2.0 * MARGIN) / q->cols, 274.0 / q->rows);
    double left = (PAPER_WIDTH - step * q->cols) / 2, top = 753;
    put(page, "q 0.4 w 0.42 0.4 0.36 RG\n");
    for (int r = 0; r < q->rows; ++r)
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
    put(page, "Q\n");
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
            line(&page, font, y, 17, text, title);
            map_quilt(&page, q);
            text = title + 1;
            y = 456;
        }
        while (page.ok && *text && y >= MARGIN) {
            const char *end = line_end(font, text);
            if (end != text) line(&page, font, y, BODY_SIZE, text, end);
            text = end + (*end == '\n' || *end == ' ');
            y -= LINE_HEIGHT;
        }
        int id = FIRST_PAGE + count++ * 2;
        object(out, offset, id);
        put(out, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d]"
                 " /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>\nendobj\n",
                 PAPER_WIDTH, PAPER_HEIGHT, id + 1);
        if (page.ok) stream_object(out, offset, id + 1, pdf_data(page.file), (size_t)SDL_GetIOSize(page.file), false);
        else out->ok = false;
        SDL_CloseIO(page.file);
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
        put(&out, "<< /Type /Catalog /Pages 2 0 R /Lang (%s) >>\nendobj\n", language_tag[prefs.language]);
        font_objects(&out, offset, &font, data, length);
        int count = pages(&out, offset, &font, q, text);
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
