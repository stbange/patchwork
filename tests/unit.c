#define SDL_MAIN_USE_CALLBACKS 1
#define SDL_MAIN_NOIMPL
#include <SDL3/SDL_main.h>
#undef main
#include "app.h"
#include "storage.h"
#include "pdf.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const Preferences english = {ENGLISH, INCHES};

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

static void test_quilt(void)
{
    Quilt q = garden_path();
    int counts[MAX_FABRICS];
    count_pieces(&q, counts);
    CHECK(memcmp(counts, (int[MAX_FABRICS]){8, 4, 4, 8, 1}, sizeof counts) == 0);
    CHECK(!paint(&q, 0, 0, 0, 2) && !paint(&q, 0, 0, 0, STARTER_FABRICS));
    CHECK(!paint(&q, -1, 0, 0, 1) && !paint(&q, 1, q.cols, 0, 1));
    CHECK(paint(&q, 2, 2, 0, 1) && q.cell[2][2].fabric[0] == 1);
    count_pieces(&q, counts);
    CHECK(counts[1] == 5 && counts[4] == 0 && counts[0] == 8);
    CHECK(!resize_quilt(&q, 5, 5, 4 * INCH));
    CHECK(!resize_quilt(&q, 2, 5, 4 * INCH) && !resize_quilt(&q, 5, 10, 4 * INCH));
    CHECK(!resize_quilt(&q, 5, 5, 1 * INCH) && !resize_quilt(&q, 5, 5, MAX_SIZE + 1));
    CHECK(resize_quilt(&q, 3, 7, 8 * INCH) && q.cell[2][2].fabric[0] == 1 && q.cell[2][5].fabric[0] == 0);
    CHECK(resize_quilt(&q, 5, 5, 2 * INCH) && q.cell[4][0].fabric[0] == 0);
}

static void test_history(void)
{
    History h = {0};
    Quilt q = garden_path();
    h.steps[0] = q;
    CHECK(!record(&h, &q) && !travel(&h, -1) && !travel(&h, 1));
    CHECK(resize_quilt(&q, 3, 3, 4 * INCH) && record(&h, &q));
    CHECK(travel(&h, -1) && h.steps[h.at].rows == 5 && h.steps[h.at].cell[4][4].fabric[0] == 2);
    CHECK(travel(&h, 1) && h.steps[h.at].rows == 3);
    CHECK(travel(&h, -1) && !travel(&h, 2));
    q = h.steps[h.at];
    CHECK(paint(&q, 0, 0, 0, 4) && record(&h, &q) && !travel(&h, 1));
    for (int i = 0; i < 2 * HISTORY; ++i) {
        CHECK(paint(&q, 0, 0, 0, i % 2) && record(&h, &q));
    }
    int count = 0;
    while (travel(&h, -1)) ++count;
    CHECK(count == HISTORY - 1);
}

static void test_format(void)
{
    Quilt original = garden_path(), decoded = {0};
    char data[PLAN_BYTES], text[SHEET_BYTES];
    size_t size = encode_plan(&original, data);
    CHECK(strcmp(data, "PATCHWORK 1\n5 5 4\nCADAC\nADBDA\nDBEBD\nADBDA\nCADAC\n") == 0);
    CHECK(decode_plan(data, size, &decoded) && quilt_equal(&decoded, &original));
    for (size_t i = 0; i < size; ++i) CHECK(!decode_plan(data, i, &decoded));
    CHECK(!decode_plan(data, size + 1, &decoded));
    const int offsets[] = {10, 12, 13, 14, 16, 17, 18, 23};
    for (size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
        char *c = &data[offsets[i]], saved = *c;
        *c = '!';
        CHECK(!decode_plan(data, size, &decoded) && quilt_equal(&decoded, &original));
        *c = saved;
    }
    CHECK(cutting_sheet(&original, english, text));
    CHECK(strstr(text, "20 x 20 in (50.8 x 50.8 cm)") && strstr(text, "25 squares at 4.5 x 4.5 in"));
    CHECK(strstr(text, "Honey check     1") && strstr(text, "C A D A C\nA D B D A\n"));
    CHECK(resize_quilt(&original, MAX_SIDE, MAX_SIDE, MAX_SIZE));
    size = encode_plan(&original, data);
    CHECK(size < PLAN_BYTES && decode_plan(data, size, &decoded));
    CHECK(cutting_sheet(&decoded, english, text));
    CHECK(strstr(text, "81 squares at 8.5 x 8.5 in") && strlen(text) < SHEET_BYTES);
}

static int design_named(const char *name)
{
    for (int id = 1; id <= PATTERN_COUNT; ++id)
        if (strcmp(patterns[id].name[ENGLISH], name) == 0) return id;
    CHECK(false);
    return 0;
}

static void check_block(const Pattern *pattern)
{
    CHECK(pattern->count > 0 && pattern->count <= MAX_PIECES);
    int twice_area = 0;
    for (int p = 0; p < pattern->count; ++p) {
        const Patch *patch = &pattern->patches[p];
        int n = patch->vertices, x = 0, y = 0;
        CHECK(n >= 3 && n <= MAX_VERTICES && patch->fabric < STARTER_FABRICS);
        for (int v = 0; v < n; ++v) {
            Point a = patch->point[v], b = patch->point[(v + 1) % n], c = patch->point[(v + 2) % n];
            CHECK(a.x <= BLOCK_UNITS && a.y <= BLOCK_UNITS);
            CHECK((b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x) > 0);
            twice_area += a.x * b.y - b.x * a.y;
        }
        for (int turn = 1; turn <= 4; ++turn) {
            x = y = 0;
            for (int v = 0; v < n; ++v) {
                Point rotated = turn_point(patch->point[v], turn);
                x += rotated.x;
                y += rotated.y;
            }
            CHECK(hit_patch(pattern, turn, (float)x / n, (float)y / n) == p);
        }
    }
    CHECK(twice_area == 2 * BLOCK_UNITS * BLOCK_UNITS);
    for (int y = 0; y < BLOCK_UNITS; ++y)
        for (int x = 0; x < BLOCK_UNITS; ++x) {
            int hits = 0;
            for (int p = 0; p < pattern->count; ++p) {
                Pattern single = {.count = 1, .patches = pattern->patches + p};
                hits += hit_patch(&single, 1, x + 0.219f, y + 0.357f) == 0;
            }
            if (hits != 1) fprintf(stderr, "%s at %d,%d has %d hits\n", pattern->name[ENGLISH], x, y, hits);
            CHECK(hits == 1);
        }
}

static uint64_t layout_signature(const Quilt *q)
{
    /* Normalize fabrics in order of appearance to catch layouts differing only by palette. */
    int color[MAX_FABRICS] = {0}, used = 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (int r = 0; r < q->rows; ++r)
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            const Pattern *pattern = block_pattern(block);
            for (int y = 0; y < BLOCK_UNITS; y += 3)
                for (int x = 0; x < BLOCK_UNITS; x += 3) {
                    int p = hit_patch(pattern, block->turn, x + 0.219f, y + 0.357f);
                    CHECK(p >= 0);
                    int f = block->fabric[p];
                    if (!color[f]) color[f] = ++used;
                    hash = (hash ^ (uint64_t)color[f]) * UINT64_C(1099511628211);
                }
        }
    return hash;
}

static void test_catalog(void)
{
    int ids[PATTERN_COUNT];
    CHECK(catalog_ids(TRADITIONAL, ids) == 19 && !patterns[ids[18]].modern);
    CHECK(catalog_ids(MODERN, ids) == 20 && patterns[ids[19]].modern);
    CHECK(catalog_ids(ALL_PATTERNS, ids) == PATTERN_COUNT);
    uint64_t signatures[PATTERN_COUNT * SETTING_COUNT];
    int layouts = 0;
    Quilt base = garden_path();
    CHECK(!arrange(&base, 0, REPEAT) && !arrange(&base, PATTERN_COUNT + 1, REPEAT));
    CHECK(!arrange(&base, 1, -1) && !arrange(&base, 1, SETTING_COUNT));
    CHECK(hit_patch(&patterns[0], 0, -1, 0) == -1);
    for (int id = 1; id <= PATTERN_COUNT; ++id) {
        const Pattern *pattern = &patterns[id];
        CHECK(ids[id - 1] == id);
        check_block(pattern);
        for (int other = 1; other < id; ++other)
            CHECK(strcmp(patterns[other].name[ENGLISH], pattern->name[ENGLISH]) != 0);
        for (int setting = 0; setting < SETTING_COUNT; ++setting) {
            Quilt q = base, restored = {0};
            CHECK(arrange(&q, id, setting));
            CHECK(q.pattern == id && q.setting == setting && q.rows == base.rows &&
                  q.cols == base.cols && q.size == base.size);
            uint64_t signature = layout_signature(&q);
            for (int seen = 0; seen < layouts; ++seen) {
                if (signatures[seen] == signature)
                    fprintf(stderr, "Identical layouts: %s / %s and %s / %s\n",
                            pattern->name[ENGLISH], setting_name[setting][ENGLISH], patterns[seen / SETTING_COUNT + 1].name[ENGLISH],
                            setting_name[seen % SETTING_COUNT][ENGLISH]);
                CHECK(signatures[seen] != signature);
            }
            signatures[layouts++] = signature;
            CHECK(resize_quilt(&q, MAX_SIDE, MAX_SIDE, MAX_SIZE) && arrange(&q, id, setting));
            char data[PLAN_BYTES], sheet[SHEET_BYTES];
            size_t size = encode_plan(&q, data);
            CHECK(decode_plan(data, size, &restored) && quilt_equal(&q, &restored));
            CHECK(cutting_sheet(&q, english, sheet));
            char final_piece[24];
            snprintf(final_piece, sizeof final_piece, "Piece %d:", pattern->count);
            CHECK(strstr(sheet, pattern->name[ENGLISH]) && strstr(sheet, "Row 9\n") && strstr(sheet, final_piece));
            CHECK(strlen(sheet) < SHEET_BYTES - 1 && !strstr(sheet, "nan,") && !strstr(sheet, "inf,"));
        }
    }
    CHECK(layouts == 312);
}

static void test_shaped_plan(void)
{
    Quilt q = garden_path(), restored = {0};
    int id = design_named("Pinwheel");
    CHECK(arrange(&q, id, BORDER));
    CHECK(q.cell[0][1].turn == 1 && q.cell[1][4].turn == 2 && q.cell[4][1].turn == 3);
    CHECK(q.cell[1][0].turn == 4 && q.cell[1][1].turn == 0);
    CHECK(!paint(&q, 0, 0, -1, 1) && !paint(&q, 1, 1, 1, 1));
    CHECK(paint(&q, 0, 0, 0, 1) && q.cell[0][0].fabric[1] == 0);
    int counts[MAX_FABRICS];
    count_pieces(&q, counts);
    CHECK(memcmp(counts, (int[MAX_FABRICS]){73, 1, 0, 63, 0}, sizeof counts) == 0);
    char data[PLAN_BYTES], sheet[SHEET_BYTES];
    size_t size = encode_plan(&q, data);
    CHECK(strncmp(data, "PATCHWORK 2\n5 5 4 05 6\n1BAADADDA ", 33) == 0);
    CHECK(decode_plan(data, size, &restored) && quilt_equal(&q, &restored));
    for (size_t i = 0; i < size; ++i) CHECK(!decode_plan(data, i, &restored));
    CHECK(!decode_plan(data, size + 1, &restored));
    const int offsets[] = {11, 17, 18, 19, 20, 21, 22, 23, 24, 32, 33};
    for (size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
        char *c = &data[offsets[i]], value = *c;
        *c = '!';
        CHECK(!decode_plan(data, size, &restored) && quilt_equal(&q, &restored));
        *c = value;
    }
    data[19] = '0';
    CHECK(!decode_plan(data, size, &restored));
    data[19] = '5';
    data[21] = '8';
    CHECK(!decode_plan(data, size, &restored));
    CHECK(resize_quilt(&q, 6, 4, 4 * INCH) && q.cell[4][1].turn == 3 && q.cell[4][1].fabric[0] == 3 &&
          q.cell[5][0].turn == 0 && q.cell[5][0].fabric[0] == 0);
    CHECK(cutting_sheet(&restored, english, sheet));
    CHECK(strstr(sheet, "Pinwheel / Border") && strstr(sheet, "Cut 137 pieces for 25 blocks"));
    CHECK(strstr(sheet, "Template 1: Square, 4.500 x 4.500 in\n  Cut  A 9"));
    CHECK(strstr(sheet, "Template 2: Triangle, 2.854 x 2.854 in\n  Cut  A 64  B 1  D 63"));
    CHECK(strstr(sheet, "Cut points: (0.000, 0.000) (2.854, 0.000) (2.854, 2.854)"));
    CHECK(strstr(sheet, "[R0 B2 A2") && strstr(sheet, "[R90") && strstr(sheet, "[- A1]"));
    CHECK(strstr(sheet, "Piece 1: (0.000, 0.000) (2.000, 0.000) (2.000, 2.000)"));
    CHECK(arrange(&q, design_named("Log Cabin"), REPEAT));
    CHECK(cutting_sheet(&q, english, sheet));
    CHECK(strstr(sheet, "Rectangle, 1.500 x 1.000 in"));
}

static void test_storage(const char *path)
{
    Quilt q = garden_path(), other = {0};
    CHECK(load_quilt(NULL, &other) == STORE_BAD && !save_quilt(NULL, &q));
    CHECK(load_quilt(path, &other) == STORE_MISSING);
    CHECK(save_quilt(path, &q) && load_quilt(path, &other) == STORE_OK);
    CHECK(quilt_equal(&q, &other));
    CHECK(paint(&q, 0, 0, 0, 1) && save_quilt(path, &q));
    CHECK(load_quilt(path, &other) == STORE_OK && other.cell[0][0].fabric[0] == 1);
    CHECK(SDL_SaveFile(path, "", 0) && load_quilt(path, &other) == STORE_BAD);
    char too_big[PLAN_BYTES] = {0};
    CHECK(SDL_SaveFile(path, too_big, sizeof too_big) && load_quilt(path, &other) == STORE_BAD);
    CHECK(SDL_RemovePath(path));
}

static void test_preferences(const char *path)
{
    Preferences prefs;
    CHECK(language_from_tag("de-DE") == GERMAN && language_from_tag("PL_pl") == POLISH);
    CHECK(language_from_tag("EN") == ENGLISH && language_from_tag("eng") == LANGUAGE_COUNT);
    CHECK(language_from_tag(NULL) == LANGUAGE_COUNT && language_from_tag("f") == LANGUAGE_COUNT);
    CHECK(!parse_preferences("de cm\n", &prefs) && !parse_preferences("de:cm", &prefs));
    CHECK(!parse_preferences("fr cm", &prefs) && !parse_preferences("pl mm", &prefs));
    CHECK(parse_preferences("pl in", &prefs) && prefs.language == POLISH && prefs.unit == INCHES);
    CHECK(SDL_SetHintWithPriority(SDL_HINT_PREFERRED_LOCALES, "fr_FR,de_DE,pl_PL", SDL_HINT_OVERRIDE));
    prefs = load_preferences(path);
    CHECK(prefs.language == GERMAN && prefs.unit == CENTIMETRES);
    CHECK(save_preferences(path, (Preferences){POLISH, INCHES}));
    prefs = load_preferences(path);
    CHECK(prefs.language == POLISH && prefs.unit == INCHES);
    CHECK(SDL_SaveFile(path, "pl mm", 5));
    CHECK(load_preferences(path).language == GERMAN);
    CHECK(SDL_SetHintWithPriority(SDL_HINT_PREFERRED_LOCALES, "fr_FR", SDL_HINT_OVERRIDE));
    CHECK(load_preferences(NULL).language == ENGLISH);
    CHECK(SDL_SetHintWithPriority(SDL_HINT_PREFERRED_LOCALES, "en_US", SDL_HINT_OVERRIDE));
    CHECK(load_preferences(path).unit == INCHES);
    CHECK(!save_preferences(NULL, prefs) && SDL_RemovePath(path));
    for (int lang = 0; lang < LANGUAGE_COUNT; ++lang)
        for (int key = 0; key < TEXT_COUNT; ++key) {
            const char *text = tr((Language)lang, (Text)key);
            CHECK(text && *text);
            for (Uint32 code; (code = SDL_StepUTF8(&text, NULL));)
                CHECK(code == '\n' || (code >= 32 && code <= 0x17f));
        }
}

static Quilt sampler(void)
{
    Quilt q = garden_path();
    CHECK(resize_quilt(&q, 5, 5, 10 * CM));
    place_block(&q.cell[0][0], design_named("Flying Geese"), 2);
    place_block(&q.cell[3][1], design_named("Storm at Sea"), 4);
    q.seams = BRICK_ROWS;
    palette_colors(15, q.color);
    q.color[2] = 0x0345ab;
    return q;
}

static void test_custom_quilt(void)
{
    CHECK(parse_size("10,05", CENTIMETRES) == 10050);
    CHECK(parse_size("4.125", INCHES) == 10478 && parse_size("5", CENTIMETRES) == MIN_SIZE);
    CHECK(parse_size("8", INCHES) == MAX_SIZE && parse_size("20.32", CENTIMETRES) == MAX_SIZE);
    CHECK(parse_size("", INCHES) < 0 && parse_size("10.", CENTIMETRES) < 0);
    CHECK(parse_size("10.00005", CENTIMETRES) < 0 && parse_size("100", CENTIMETRES) < 0);
    CHECK(parse_size("10.2x", CENTIMETRES) < 0 && parse_size("10..3", INCHES) < 0);
    CHECK(parse_size("4.20", CENTIMETRES) < 0 && parse_size("8.01", INCHES) < 0);
    CHECK(parse_color("#0345aB") == 0x0345ab && parse_color("FFFFFF") == 0xffffff);
    CHECK(parse_color("000000") == 0 && parse_color("FF000") < 0 && parse_color(":AFFFF") < 0);
    CHECK(parse_color("12345g") < 0);
    uint32_t palettes[PALETTE_COUNT][STARTER_FABRICS];
    for (int n = 0; n < PALETTE_COUNT; ++n) {
        palette_colors(n, palettes[n]);
        char name[PALETTE_NAME_BYTES];
        palette_name(n, POLISH, name);
        CHECK(strstr(name, " / "));
        for (int i = 0; i < n; ++i) CHECK(memcmp(palettes[i], palettes[n], sizeof palettes[n]) != 0);
        for (int f = 0; f < STARTER_FABRICS; ++f) CHECK(palettes[n][f] <= 0xffffff);
    }

    Quilt q = sampler(), copy = {0};
    CHECK(mixed_quilt(&q) && strcmp(quilt_name(&q, GERMAN), "Sampler-Quilt") == 0);
    BlockBounds a = block_bounds(&q, 1, 0), b = block_bounds(&q, 1, 4), plain = block_bounds(&q, 0, 0);
    CHECK(a.w == 1.5 && b.w == 0.5 && b.x + b.w == 5 && plain.w == 1);
    q.seams = BRICK_COLUMNS;
    a = block_bounds(&q, 0, 1); b = block_bounds(&q, 4, 1); plain = block_bounds(&q, 0, 0);
    CHECK(a.h == 1.5 && b.h == 0.5 && b.y + b.h == 5 && plain.h == 1);
    q.seams = BRICK_ROWS;
    char data[PLAN_BYTES], sheet[SHEET_BYTES];
    size_t size = encode_plan(&q, data);
    CHECK(strncmp(data, "PATCHWORK 3\n5 5 10000 00 0 1\n", 29) == 0 && strstr(data, "0345AB"));
    CHECK(decode_plan(data, size, &copy) && quilt_equal(&q, &copy));
    for (size_t i = 0; i < size; ++i) CHECK(!decode_plan(data, i, &copy));
    const char *bad = strchr(data + 29, '\n') + 1;
    size_t first_block = (size_t)(bad - data);
    size_t corrupt[] = {10, 16, 25, 27, 29, 35, first_block, first_block + 2, first_block + 3};
    for (size_t i = 0; i < sizeof corrupt / sizeof corrupt[0]; ++i) {
        char value = data[corrupt[i]];
        data[corrupt[i]] = '?';
        CHECK(!decode_plan(data, size, &copy) && quilt_equal(&q, &copy));
        data[corrupt[i]] = value;
    }
    data[first_block + 2] = '0';
    CHECK(!decode_plan(data, size, &copy));
    data[first_block] = data[first_block + 1] = '0'; data[first_block + 2] = '1';
    CHECK(!decode_plan(data, size, &copy));
    CHECK(cutting_sheet(&q, (Preferences){POLISH, CENTIMETRES}, sheet));
    CHECK(strstr(sheet, "PLAN CIĘCIA") && strstr(sheet, "50 × 50 cm") && strstr(sheet, "0,635 cm"));
    CHECK(strstr(sheet, "16,270 × 11,270 cm") && strstr(sheet, "Lecące gęsi") && strstr(sheet, "Sztorm na morzu"));
    CHECK(strstr(sheet, "#0345AB") && strstr(sheet, "Przesunięte rzędy") && strstr(sheet, "; 0,000)"));
    CHECK(cutting_sheet(&q, (Preferences){GERMAN, INCHES}, sheet));
    CHECK(strstr(sheet, "ZUSCHNITTPLAN") && strstr(sheet, "19,685 × 19,685 Zoll"));
    CHECK(strstr(sheet, "1/4 Zoll") && strstr(sheet, "Sturm auf See"));
    CHECK(strstr(seam_measure((Preferences){ENGLISH, CENTIMETRES}), "0.635"));
}

typedef struct {
    const Quilt *q;
    Outline *shape;
    bool seen[LAYER_SLOTS];
    int count, bridges;
    double area;
} Shapes;

static double area2(XY a, XY b, XY c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static void check_outline(const Outline *s, void *data)
{
    Shapes *v = data;
    CHECK(s->n >= 3 && s->n <= OUTLINE_POINTS && s->id >= 0 && s->id < LAYER_SLOTS);
    CHECK(s->fabric >= 0 && s->fabric < v->q->fabrics);
    if (s->bridge) {
        CHECK(v->seen[s->id]);
        ++v->bridges;
    } else {
        CHECK(!v->seen[s->id]);
        v->seen[s->id] = true;
        v->shape[v->count++] = *s;
    }
    double area = 0, triangles = 0;
    for (int i = 0; i < s->n; ++i) {
        XY p = s->p[i], next = s->p[(i + 1) % s->n];
        CHECK(p.x >= -1e-8 && p.y >= -1e-8 && p.x <= v->q->cols + 1e-8 && p.y <= v->q->rows + 1e-8);
        CHECK(hypot(next.x - p.x, next.y - p.y) > 1e-9);
        area += area2(s->p[0], p, next);
    }
    int index[3 * (OUTLINE_POINTS - 2)];
    int n = outline_mesh(s, index);
    for (int i = 0; i < n; i += 3) {
        double triangle = area2(s->p[index[i]], s->p[index[i + 1]], s->p[index[i + 2]]);
        CHECK(triangle > 0);
        triangles += triangle;
    }
    if (fabs(triangles - area) >= 1e-7)
        fprintf(stderr, "Mesh mismatch in %s, piece %d: %.12f vs %.12f\n", tr(ENGLISH, s->name), s->id, area,
                triangles);
    CHECK(area > 0 && fabs(triangles - area) < 1e-7);
    CHECK(inside_outline(s, outline_label(s)));
    v->area += area / 2;
}

static void test_whole_geometry(void)
{
    Quilt q = garden_path();
    Outline *shapes = calloc(LAYER_SLOTS, sizeof *shapes);
    CHECK(shapes);
    for (int id = WHOLECLOTH; id < WHOLE_COUNT; ++id) {
        Layer layer = id < VINE ? FOUNDATION : APPLIQUE;
        set_whole_design(&q, layer, (WholeDesign)id);
        for (int size = MIN_SIDE; size <= MAX_SIDE; ++size) {
            resize_quilt(&q, MAX_SIDE + MIN_SIDE - size, size, MIN_SIZE);
            Shapes v = {.q = &q, .shape = shapes};
            visit_layer(&q, layer, check_outline, &v);
            CHECK(v.count > 0 && (id != RIBBONS || (v.count == q.cols + q.rows && v.bridges > 0)));
            if (layer == FOUNDATION) {
                CHECK(fabs(v.area - q.cols * q.rows) < 1e-7);
                for (double y = .0787; y < q.rows; y += .21)
                    for (double x = .1203; x < q.cols; x += .193) {
                        int containing = 0;
                        for (int i = 0; i < v.count; ++i) containing += inside_outline(&shapes[i], (XY){x, y});
                        CHECK(containing == 1);
                    }
                for (int i = 0; i < v.count; ++i) {
                    XY cut[MAX_CUT_POINTS];
                    int n = cut_outline(&shapes[i], (double)SEAM_ALLOWANCE / q.size, cut);
                    for (int j = 0; j < n; ++j) CHECK(isfinite(cut[j].x) && isfinite(cut[j].y));
                }
            }
        }
    }
    free(shapes);
    set_whole_design(&q, FOUNDATION, HEX_FIELD);
    int hex = hit_whole(&q, FOUNDATION, (XY){.91, .12});
    CHECK(hex >= 0 && hex == hit_whole(&q, FOUNDATION, (XY){1.09, .12}));
    set_whole_design(&q, FOUNDATION, CLAMSHELL);
    int shell = hit_whole(&q, FOUNDATION, (XY){.91, .23});
    CHECK(shell >= 0 && shell == hit_whole(&q, FOUNDATION, (XY){1.09, .23}));
    CHECK(shell != hit_whole(&q, FOUNDATION, (XY){1.38, .12}));
    set_whole_design(&q, APPLIQUE, RIBBONS);
    CHECK(hit_whole(&q, APPLIQUE, (XY){.5, .5}) == 0 && hit_whole(&q, APPLIQUE, (XY){1.5, .5}) == MAX_SIDE + 1);
    CHECK(hit_whole(&q, APPLIQUE, (XY){.5, 1.5}) == MAX_SIDE && hit_whole(&q, APPLIQUE, (XY){1.5, 1.5}) == 1);
    CHECK(hit_whole(&q, APPLIQUE, (XY){2.5, .5}) == 0 && hit_whole(&q, APPLIQUE, (XY){0, 0}) == -1);
}

static void test_scraps_and_layers(void)
{
    Quilt q = sampler(), restored = {0};
    CHECK(extra_fabric_color(5) != extra_fabric_color(6));
    CHECK(!add_fabric(&q, 0x1000000) && q.fabrics == STARTER_FABRICS);
    for (int f = STARTER_FABRICS; f < MAX_FABRICS; ++f) CHECK(add_fabric(&q, 0x105000u + f));
    CHECK(!add_fabric(&q, 0) && q.fabrics == MAX_FABRICS);
    CHECK(paint(&q, 0, 0, 0, 25));
    palette_colors(32, q.color);
    CHECK(q.color[25] == 0x105019 && q.cell[0][0].fabric[0] == 25);
    set_whole_design(&q, APPLIQUE, VINE);
    char sheet[SHEET_BYTES], data[PLAN_BYTES];
    CHECK(cutting_sheet(&q, english, sheet) && strstr(sheet, "Flowering Vine") && strstr(sheet, "Flying Geese"));
    CHECK(strstr(sheet, "Stem") && strstr(sheet, "Branch") && strstr(sheet, "raw-edge") &&
          strstr(sheet, "Z  #105019"));
    set_whole_design(&q, FOUNDATION, CLAMSHELL);
    int id = next_whole(&q, FOUNDATION, -1, 1);
    int last = next_whole(&q, FOUNDATION, id, -1);
    CHECK(id >= 0 && last != id && next_whole(&q, FOUNDATION, last, 1) == id);
    CHECK(paint_whole(&q, FOUNDATION, id, 25) && !paint_whole(&q, FOUNDATION, id, 25));
    CHECK(!paint_whole(&q, FOUNDATION, -1, 0) && !paint_whole(&q, FOUNDATION, LAYER_SLOTS - 1, 0));
    CHECK(!paint_whole(&q, FOUNDATION, id, q.fabrics));
    set_whole_design(&q, APPLIQUE, RIBBONS);
    CHECK(paint_whole(&q, APPLIQUE, 0, 25));
    int counts[MAX_FABRICS];
    count_pieces(&q, counts);
    CHECK(counts[25] == 2);
    size_t n = encode_plan(&q, data);
    CHECK(n < PLAN_BYTES && strstr(data, "PATCHWORK 4\n") && strstr(data, "26 4 7\n"));
    CHECK(decode_plan(data, n, &restored) && quilt_equal(&q, &restored));
    for (size_t i = 0; i < n; ++i) CHECK(!decode_plan(data, i, &restored));
    size_t invalid[] = {28, 29, 31, 32, 33, 34, n - 2, n - 1};
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        char old = data[invalid[i]];
        data[invalid[i]] = '?';
        CHECK(!decode_plan(data, n, &restored) && quilt_equal(&q, &restored));
        data[invalid[i]] = old;
    }
    CHECK(cutting_sheet(&q, english, sheet) && strstr(sheet, "Z  #105019         2") &&
          strstr(sheet, "Ribbon Lattice"));
    CHECK(strstr(sheet, "A1  Ribbon  /  Z") && strstr(sheet, "solid stitching lines") && strstr(sheet, "Weave"));
    CHECK(paint_whole(&q, APPLIQUE, 0, 3) && !q.dye[APPLIQUE][0]);
    set_whole_design(&q, APPLIQUE, BLOCK_TOP);
    CHECK(!q.applique && !memcmp(q.cell, restored.cell, sizeof q.cell));
    set_whole_design(&q, FOUNDATION, BLOCK_TOP);
    CHECK(q.cell[0][0].fabric[0] == 25);
    CHECK(resize_quilt(&q, MAX_SIDE, MAX_SIDE, MAX_SIZE) && q.fabrics == MAX_FABRICS);
    CHECK(arrange(&q, design_named("Storm at Sea"), REPEAT));
    set_whole_design(&q, FOUNDATION, TUMBLING);
    set_whole_design(&q, APPLIQUE, KOI);
    n = encode_plan(&q, data);
    CHECK(n < PLAN_BYTES && decode_plan(data, n, &restored) && quilt_equal(&q, &restored));
    CHECK(cutting_sheet(&q, (Preferences){POLISH, CENTIMETRES}, sheet));
    CHECK(strstr(sheet, "Staw z karpiami koi") && strstr(sheet, "Tułów karpia"));
}

static bool pdf_has(const void *source, size_t count, const char *needle)
{
    const char *text = source;
    size_t length = strlen(needle);
    for (size_t i = 0; i + length <= count; ++i)
        if (memcmp(text + i, needle, length) == 0) return true;
    return false;
}

static void test_pdf(void)
{
    Quilt q = sampler();
    SDL_IOStream *pdf = make_pdf(&q, (Preferences){POLISH, CENTIMETRES});
    CHECK(pdf);
    size_t size = (size_t)SDL_GetIOSize(pdf);
    const char *data = pdf_data(pdf);
    CHECK(size > 240000 && data && memcmp(data, "%PDF-1.4\n", 9) == 0);
    CHECK(pdf_has(data, size, "/Lang (pl)") && pdf_has(data, size, "/ToUnicode 7 0 R"));
    CHECK(pdf_has(data, size, "startxref\n") && pdf_has(data, size, "<0119>") && pdf_has(data, size, "h B\n"));
    CHECK(memcmp(data + size - 6, "%%EOF\n", 6) == 0 && SDL_CloseIO(pdf));
    set_whole_design(&q, FOUNDATION, CLAMSHELL);
    set_whole_design(&q, APPLIQUE, RIBBONS);
    pdf = make_pdf(&q, english);
    CHECK(pdf);
    size = (size_t)SDL_GetIOSize(pdf);
    CHECK(pdf_has(pdf_data(pdf), size, "[3 2] 0 d") && pdf_has(pdf_data(pdf), size, "44 57 m 116.000 57 l S"));
    CHECK(SDL_CloseIO(pdf));
}

static void key(App *a, SDL_Keycode code, SDL_Keymod mod)
{
    SDL_Event e = {0};
    e.type = SDL_EVENT_KEY_DOWN; e.key.key = code; e.key.mod = mod;
    CHECK(SDL_AppEvent(a, &e) == SDL_APP_CONTINUE);
}

static void type(App *a, const char *text)
{
    SDL_Event e = {0};
    e.type = SDL_EVENT_TEXT_INPUT;
    e.text.text = text;
    CHECK(SDL_AppEvent(a, &e) == SDL_APP_CONTINUE);
}

static void click(App *a, float x, float y)
{
    SDL_Event e = {0};
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN; e.button.button = SDL_BUTTON_LEFT;
    CHECK(SDL_RenderCoordinatesToWindow(a->renderer, x, y, &e.button.x, &e.button.y));
    CHECK(SDL_AppEvent(a, &e) == SDL_APP_CONTINUE);
}

static void button(App *a, Control id)
{
    SDL_FRect b = control_box[id];
    click(a, b.x + b.w / 2, b.y + b.h / 2);
}

static void choose_box(App *a, SDL_FRect b)
{
    click(a, b.x + b.w / 2, b.y + b.h / 2);
}

static App *start(char *path)
{
    void *state = NULL;
    char *args[] = {"patchwork", "--project", path, NULL};
    CHECK(SDL_AppInit(&state, 3, args) == SDL_APP_CONTINUE);
    App *a = state;
    a->prefs = english;
    return a;
}

static void test_inputs(App *a)
{
    key(a, SDLK_5, 0);
    CHECK(a->fabric == 4);
    key(a, SDLK_LEFT, 0); key(a, SDLK_UP, 0); key(a, SDLK_RIGHT, 0); key(a, SDLK_DOWN, 0);
    CHECK(a->row == 1 && a->col == 1);
    key(a, SDLK_SPACE, 0);
    CHECK(a->history.steps[a->history.at].cell[1][1].fabric[0] == 4);
    key(a, SDLK_Z, SDL_KMOD_CTRL); CHECK(a->history.at == 0);
    key(a, SDLK_Y, SDL_KMOD_CTRL); CHECK(a->history.at == 1);
    button(a, UNDO); key(a, SDLK_Z, SDL_KMOD_CTRL | SDL_KMOD_SHIFT);
    CHECK(a->history.at == 1);
    key(a, SDLK_R, 0); key(a, SDLK_C, 0); key(a, SDLK_EQUALS, 0);
    Quilt q = a->history.steps[a->history.at];
    CHECK(q.rows == 6 && q.cols == 6 && q.size == 5 * INCH);
    key(a, SDLK_R, SDL_KMOD_SHIFT); key(a, SDLK_C, SDL_KMOD_SHIFT); key(a, SDLK_MINUS, 0);
    key(a, SDLK_UNKNOWN, 0); key(a, SDLK_C, SDL_KMOD_CTRL);
    q = a->history.steps[a->history.at];
    CHECK(q.rows == 5 && q.cols == 5 && q.size == 4 * INCH);
    button(a, SWATCH_B);
    SDL_FRect b = quilt_box(&q, design_mat);
    click(a, b.x + b.w / 2, b.y + b.h / 2);
    CHECK(a->row == 2 && a->col == 2 && a->history.steps[a->history.at].cell[2][2].fabric[0] == 1);
    click(a, b.x + b.w, b.y + b.h); click(a, 0, 0);
    CHECK(a->row == 2 && a->col == 2);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE && !a->dirty);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    button(a, SHEET); CHECK(a->screen == SHEET);
    key(a, SDLK_5, 0); CHECK(a->fabric == 1);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    key(a, SDLK_RETURN, 0);
    char *sheet = SDL_GetClipboardText();
    CHECK(sheet && strstr(sheet, "25 squares at 4.5 x 4.5 in"));
    SDL_free(sheet);
    button(a, SHARE);
    CHECK(a->status == SHEET_COPIED);
    key(a, SDLK_TAB, 0); CHECK(a->screen == DESIGN);
    key(a, SDLK_S, 0); CHECK(a->status == SAVED);
    SDL_Event e = {0}; e.type = SDL_EVENT_WINDOW_EXPOSED;
    CHECK(SDL_AppEvent(a, &e) == SDL_APP_CONTINUE && a->dirty);
    e.type = SDL_EVENT_MOUSE_MOTION; CHECK(SDL_AppEvent(a, &e) == SDL_APP_CONTINUE);
    e.type = SDL_EVENT_QUIT; CHECK(SDL_AppEvent(a, &e) == SDL_APP_SUCCESS);
}

static void test_drawing(App *a)
{
    SDL_Surface *surface = SDL_CreateSurface(WALL_WIDTH, WALL_HEIGHT, SDL_PIXELFORMAT_RGBA32);
    CHECK(surface);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    CHECK(renderer && !open_view(renderer, "missing-assets/"));
    App frame = *a;
    frame.renderer = renderer;
    frame.view = open_view(renderer, SDL_GetBasePath());
    CHECK(frame.view);
    frame.cursor = false;
    SDL_FRect b = quilt_box(&frame.history.steps[frame.history.at], design_mat);
    int x = (int)b.x + 7, y = (int)b.y + 9;
    Uint8 r, g, blue, alpha;
    draw_app(&frame);
    CHECK(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &blue, &alpha) && g > r);
    frame.history.steps[frame.history.at].cell[0][0].fabric[0] = 1;
    draw_app(&frame);
    CHECK(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &blue, &alpha) && r > g);
    frame.screen = SHEET;
    draw_app(&frame);
    CHECK(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &blue, &alpha) && r > 200);
    close_view(frame.view);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
}

static void test_library_ui(App *a)
{
    Quilt original = a->history.steps[a->history.at];
    int before = a->history.at;
    key(a, SDLK_P, 0);
    CHECK(a->screen == LIBRARY && a->chosen == 0 && a->setting == REPEAT);
    button(a, FILTER_MODERN);
    CHECK(a->filter == MODERN && a->chosen == 0);
    key(a, SDLK_LEFT, 0); key(a, SDLK_RIGHT, 0);
    key(a, SDLK_DOWN, 0); key(a, SDLK_UP, 0);
    CHECK(a->chosen == 1);
    button(a, PREV_PAGE); CHECK(a->chosen == 1);
    key(a, SDLK_PAGEDOWN, 0); CHECK(a->chosen == 6);
    button(a, NEXT_PAGE); CHECK(a->chosen == 12);
    button(a, NEXT_PAGE); CHECK(a->chosen == 18);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    key(a, SDLK_PAGEUP, 0); CHECK(a->chosen == 12);
    key(a, SDLK_1, 0); CHECK(a->filter == ALL_PATTERNS && a->chosen == 0);
    key(a, SDLK_2, 0); CHECK(a->filter == TRADITIONAL && a->chosen == 0);
    key(a, SDLK_3, 0); CHECK(a->filter == MODERN && a->chosen == 0);
    button(a, FILTER_TRADITIONAL);
    choose_box(a, pattern_card(4));
    CHECK(a->chosen == 4 && a->history.at == before);
    key(a, SDLK_LEFTBRACKET, 0); CHECK(a->setting == MEDALLION);
    key(a, SDLK_RIGHTBRACKET, 0); CHECK(a->setting == REPEAT);
    choose_box(a, setting_box(BORDER));
    CHECK(a->setting == BORDER && a->history.at == before);
    key(a, SDLK_ESCAPE, 0); CHECK(a->screen == DESIGN && a->history.at == before);
    button(a, LIBRARY);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    key(a, SDLK_RETURN, 0);
    CHECK(a->screen == DESIGN && a->history.at == before + 1);
    Quilt q = a->history.steps[a->history.at];
    CHECK(q.pattern == design_named("Pinwheel") && q.setting == BORDER);
    CHECK(q.rows == original.rows && q.cols == original.cols && q.size == original.size);
    CHECK(load_quilt(a->path, &q) == STORE_OK && q.cell[0][0].fabric[0] == 3);
    SDL_FRect b = quilt_box(&q, design_mat);
    float side = b.w / q.cols;
    click(a, b.x + side * 0.4f, b.y + side * 0.1f);
    CHECK(a->patch == 0 && a->history.steps[a->history.at].cell[0][0].fabric[0] == 1);
    CHECK(a->history.steps[a->history.at].cell[0][0].fabric[1] == 0);
    key(a, SDLK_RIGHTBRACKET, 0); CHECK(a->patch == 1);
    key(a, SDLK_LEFTBRACKET, 0); CHECK(a->patch == 0);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    button(a, SHEET); CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    button(a, SHARE);
    char *text = SDL_GetClipboardText();
    CHECK(text && strstr(text, "Pinwheel / Border") && strstr(text, "Triangle, 2.854 x 2.854"));
    SDL_free(text);
    button(a, UNDO); button(a, UNDO);
    CHECK(a->history.at == before && quilt_equal(&a->history.steps[before], &original));
    button(a, REDO);
    CHECK(a->history.steps[a->history.at].pattern == design_named("Pinwheel"));
    button(a, UNDO);
    button(a, DESIGN);
}

static void test_whole_ui(App *a)
{
    a->prefs = english;
    button(a, DESIGN);
    choose_box(a, fabric_nav(ADD_SWATCH, DESIGN));
    CHECK(a->adding && a->editor == EDIT_FABRIC && SDL_AppIterate(a) == SDL_APP_CONTINUE);
    key(a, SDLK_ESCAPE, 0);
    CHECK(a->history.steps[a->history.at].fabrics == 5);
    key(a, SDLK_N, 0);
    type(a, "0018AB");
    button(a, EDIT_APPLY);
    CHECK(a->fabric == 5 && a->fabric_page == 1 && a->history.steps[a->history.at].fabrics == 6);
    key(a, SDLK_3, 0);
    CHECK(a->fabric == 5);
    key(a, SDLK_COMMA, 0);
    CHECK(a->fabric_page == 0);
    button(a, SWATCH_C);
    CHECK(a->fabric == 2);
    choose_box(a, fabric_nav(FABRIC_NEXT, DESIGN));
    key(a, SDLK_1, 0);
    CHECK(a->fabric == 5);
    button(a, UNDO);
    CHECK(a->fabric < 5 && a->fabric_page == 0);
    button(a, REDO);
    key(a, SDLK_PERIOD, 0);
    key(a, SDLK_1, 0);
    button(a, LIBRARY);
    key(a, SDLK_4, 0);
    CHECK(a->whole_library && SDL_AppIterate(a) == SDL_APP_CONTINUE);
    key(a, SDLK_RIGHT, 0);
    key(a, SDLK_RETURN, 0);
    CHECK(a->history.steps[a->history.at].foundation == TUMBLING);
    key(a, SDLK_RIGHTBRACKET, 0);
    key(a, SDLK_SPACE, 0);
    CHECK(a->history.steps[a->history.at].dye[FOUNDATION][a->whole_piece] == 6);
    key(a, SDLK_LEFT, 0);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    button(a, LIBRARY);
    choose_box(a, pattern_card(KOI - HEX_FIELD));
    button(a, APPLY);
    CHECK(a->history.steps[a->history.at].foundation == TUMBLING && a->history.steps[a->history.at].applique == KOI);
    button(a, EDIT_LAYER);
    CHECK(a->background_only);
    key(a, SDLK_W, 0);
    CHECK(!a->background_only);
    key(a, SDLK_SPACE, 0);
    CHECK(a->whole_piece >= 0 && a->selected_layer == APPLIQUE);
    button(a, LIBRARY);
    choose_box(a, pattern_card(RIBBONS - HEX_FIELD));
    button(a, BRUSH);
    const Quilt *q = &a->history.steps[a->history.at];
    CHECK(q->foundation == WHOLECLOTH && q->applique == RIBBONS);
    SDL_FRect b = quilt_box(q, design_mat);
    float step = b.w / q->cols;
    click(a, b.x + .5f * step, b.y + .5f * step);
    CHECK(a->whole_piece == 0 && a->history.steps[a->history.at].dye[APPLIQUE][0] == 6);
    click(a, b.x + .05f * step, b.y + .05f * step);
    CHECK(a->selected_layer == FOUNDATION && a->history.steps[a->history.at].dye[FOUNDATION][0] == 6);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    button(a, SHEET);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    choose_box(a, fabric_nav(FABRIC_PREV, SHEET));
    CHECK(a->fabric_page == 0);
    button(a, COLORS);
    button(a, ORIGINAL_COLORS);
    CHECK(a->history.steps[a->history.at].color[5] == 0x0018ab);
    button(a, LIBRARY);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    key(a, SDLK_X, 0);
    CHECK(!a->history.steps[a->history.at].applique);
    key(a, SDLK_G, 0);
    CHECK(a->history.steps[a->history.at].foundation == BLOCK_TOP);
    button(a, FILTER_ALL);
    button(a, BRUSH);
    click(a, b.x + .2f * step, b.y + .15f * step);
    CHECK(a->history.steps[a->history.at].foundation == BLOCK_TOP && !a->history.steps[a->history.at].applique);
}

static void test_localized_ui(App *a, const char *path)
{
    SDL_free(a->settings_path);
    a->settings_path = SDL_strdup(path);
    Quilt original = a->history.steps[0];
    key(a, SDLK_L, 0);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE && a->language_menu);
    key(a, SDLK_2, SDL_KMOD_CTRL); CHECK(a->language_menu);
    key(a, SDLK_2, 0); CHECK(a->prefs.language == GERMAN && !a->language_menu);
    button(a, LANGUAGE_MENU); button(a, LANGUAGE_PL);
    CHECK(a->prefs.language == POLISH && load_preferences(path).language == POLISH);
    button(a, LANGUAGE_MENU); click(a, 100, 110); CHECK(!a->language_menu);
    button(a, LANGUAGE_MENU); key(a, SDLK_ESCAPE, 0); CHECK(!a->language_menu);
    CHECK(quilt_equal(&original, &a->history.steps[a->history.at]) && a->history.at == 0);
    key(a, SDLK_U, 0); CHECK(a->prefs.unit == CENTIMETRES);
    key(a, SDLK_M, 0); CHECK(a->editor == EDIT_SIZE_VALUE && a->replace_input);
    key(a, SDLK_BACKSPACE, 0);
    type(a, "no"); key(a, SDLK_RETURN, 0);
    CHECK(a->invalid && a->editor == EDIT_SIZE_VALUE);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    type(a, "10");
    key(a, SDLK_RETURN, 0);
    CHECK(!a->editor && a->history.steps[a->history.at].size == 10 * CM);
    button(a, SET_SIZE); button(a, EDIT_CANCEL); CHECK(!a->editor);
    button(a, SET_SEAMS); CHECK(a->history.steps[a->history.at].seams == BRICK_ROWS);
    key(a, SDLK_P, 0);
    choose_box(a, pattern_card(2)); button(a, BRUSH);
    CHECK(a->brush == design_named("Log Cabin") && a->screen == DESIGN);
    button(a, ROTATE_BLOCK);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE && a->turn == 2);
    SDL_FRect b = quilt_box(&a->history.steps[a->history.at], design_mat);
    float side = b.w / 5;
    click(a, b.x + side * 1.2f, b.y + side * 1.2f);
    CHECK(a->row == 1 && a->col == 0 && a->history.steps[a->history.at].cell[1][0].turn == 2);
    key(a, SDLK_ESCAPE, 0); CHECK(!a->brush);
    key(a, SDLK_F, 0);
    key(a, SDLK_RIGHT, 0); key(a, SDLK_LEFT, 0); button(a, NEXT_PAGE);
    CHECK(a->palette_page == 1 && a->screen == COLORS);
    choose_box(a, palette_card(3));
    uint32_t palette[STARTER_FABRICS];
    palette_colors(13, palette);
    CHECK(memcmp(a->history.steps[a->history.at].color, palette, sizeof palette) == 0);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    key(a, SDLK_3, 0);
    CHECK(a->editor == EDIT_FABRIC && a->fabric == 2);
    choose_box(a, slider_box(1));
    CHECK((parse_color(a->input) & 0xff00) == 0x8000);
    CHECK(SDL_AppIterate(a) == SDL_APP_CONTINUE);
    key(a, SDLK_A, SDL_KMOD_CTRL);
    type(a, "1020AB"); button(a, EDIT_APPLY);
    CHECK(a->history.steps[a->history.at].color[2] == 0x1020ab);
    button(a, ORIGINAL_COLORS);
    CHECK(a->history.steps[a->history.at].color[2] == original_palette[2]);
    button(a, UNDO);
    CHECK(a->history.steps[a->history.at].color[2] == 0x1020ab);
    button(a, SHEET); button(a, SHARE);
    CHECK(a->status == SHEET_COPIED && SDL_AppIterate(a) == SDL_APP_CONTINUE);
    char *text = SDL_GetClipboardText();
    CHECK(text && strstr(text, "PLAN CIĘCIA") && strstr(text, "0,635 cm") && strstr(text, "#1020AB"));
    SDL_free(text);
    SDL_Event pdf = {.type = SDL_EVENT_USER};
    pdf.user.code = PDF_EXPORTED;
    CHECK(SDL_AppEvent(a, &pdf) == SDL_APP_CONTINUE && a->status == PDF_EXPORTED);
    CHECK(SDL_RemovePath(path));
}

int main(void)
{
    SDL_SetMainReady();
    test_quilt(); test_history(); test_format(); test_catalog(); test_shaped_plan(); test_custom_quilt(); test_whole_geometry(); test_scraps_and_layers();
    char *dir = SDL_GetPrefPath("PatchworkTests", "Unit");
    CHECK(dir);
    char *path, *bad, *prefs;
    CHECK(SDL_asprintf(&path, "%sunit-%llu.pw", dir, (unsigned long long)SDL_GetPerformanceCounter()) > 0);
    CHECK(SDL_asprintf(&bad, "%s/missing/plan", path) > 0);
    CHECK(SDL_asprintf(&prefs, "%s.settings", path) > 0);
    test_preferences(prefs);
    test_storage(path);
    App *a = start(path);
    CHECK(strcmp(SDL_GetRendererName(a->renderer), "software") == 0);
    test_inputs(a); test_drawing(a); test_library_ui(a);
    test_pdf();
    SDL_AppQuit(a, SDL_APP_SUCCESS);
    a = start(path); CHECK(a->history.steps[0].cell[2][2].fabric[0] == 1);
    SDL_AppQuit(a, SDL_APP_SUCCESS);
    CHECK(SDL_SaveFile(path, "broken", 6));
    a = start(path); key(a, SDLK_SPACE, 0);
    CHECK(a->protected_plan && a->status == PROTECTED_PLAN);
    SDL_AppQuit(a, SDL_APP_SUCCESS);
    size_t size;
    char *original = SDL_LoadFile(path, &size);
    CHECK(original && size == 6 && memcmp(original, "broken", 6) == 0);
    SDL_free(original);
    a = start(bad); key(a, SDLK_SPACE, 0);
    CHECK(a->status == SAVE_FAILED);
    SDL_AppQuit(a, SDL_APP_SUCCESS);
    void *state = NULL;
    char *args[] = {"patchwork", "--unknown", NULL};
    CHECK(SDL_AppInit(&state, 2, args) == SDL_APP_FAILURE);
    SDL_AppQuit(state, SDL_APP_FAILURE);
    CHECK(SDL_AppInit(&state, 1, args) == SDL_APP_CONTINUE);
    SDL_AppQuit(state, SDL_APP_SUCCESS);
    SDL_AppQuit(NULL, SDL_APP_FAILURE);
    CHECK(SDL_RemovePath(path));
    a = start(path);
    test_localized_ui(a, prefs);
    test_whole_ui(a);
    SDL_AppQuit(a, SDL_APP_SUCCESS);
    CHECK(SDL_RemovePath(path));
    SDL_free(prefs); SDL_free(bad); SDL_free(path); SDL_free(dir);
    SDL_Quit();
    puts("Quilt, history, format, storage, input and rendering checks passed.");
    return 0;
}
