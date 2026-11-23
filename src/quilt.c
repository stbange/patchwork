#include "quilt.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

const uint32_t original_palette[FABRICS] = {0xeee0bf, 0xb86761, 0x8a9b78, 0x4c6171, 0xd5ab62};
const char *const fabric_name[FABRICS][LANGUAGE_COUNT] = {
    {"Oat linen", "Haferbeiges Leinen", "Lniany beż"},
    {"Madder rose", "Krapp-Rosa", "Róż marzanny"},
    {"Sage sprig", "Salbeizweig", "Gałązka szałwii"},
    {"Indigo stripe", "Indigostreifen", "Paski indygo"},
    {"Honey check", "Honigkaros", "Miodowa kratka"}
};

Quilt garden_path(void)
{
    static const uint8_t sample[5][5] = {
        {2, 0, 3, 0, 2}, {0, 3, 1, 3, 0}, {3, 1, 4, 1, 3}, {0, 3, 1, 3, 0}, {2, 0, 3, 0, 2}
    };
    Quilt q = {.rows = 5, .cols = 5, .size = 4 * INCH};
    memcpy(q.color, original_palette, sizeof q.color);
    for (int r = 0; r < q.rows; ++r)
        for (int c = 0; c < q.cols; ++c)
            q.cell[r][c].fabric[0] = sample[r][c];
    return q;
}

bool paint(Quilt *q, int row, int col, int patch, int fabric)
{
    if (row < 0 || row >= q->rows || col < 0 || col >= q->cols ||
        fabric < 0 || fabric >= FABRICS)
        return false;
    Block *block = &q->cell[row][col];
    if (patch < 0 || patch >= block_pattern(block)->count || block->fabric[patch] == fabric)
        return false;
    block->fabric[patch] = (uint8_t)fabric;
    return true;
}

bool resize_quilt(Quilt *q, int rows, int cols, int size)
{
    if (rows < MIN_SIDE || rows > MAX_SIDE || cols < MIN_SIDE || cols > MAX_SIDE ||
        size < MIN_SIZE || size > MAX_SIZE ||
        (rows == q->rows && cols == q->cols && size == q->size))
        return false;
    Quilt next = {.rows = (uint8_t)rows, .cols = (uint8_t)cols, .size = (uint16_t)size,
                  .pattern = q->pattern, .setting = q->setting, .seams = q->seams};
    memcpy(next.color, q->size ? q->color : original_palette, sizeof next.color);
    size_t overlap = (size_t)(cols < q->cols ? cols : q->cols) * sizeof(Block);
    for (int r = 0; r < rows && r < q->rows; ++r)
        memcpy(next.cell[r], q->cell[r], overlap);
    *q = next;
    return true;
}

bool quilt_equal(const Quilt *a, const Quilt *b)
{
    return a->size == b->size && a->rows == b->rows && a->cols == b->cols &&
           a->pattern == b->pattern && a->setting == b->setting && a->seams == b->seams &&
           memcmp(a->color, b->color, sizeof a->color) == 0 && memcmp(a->cell, b->cell, sizeof a->cell) == 0;
}

bool record(History *h, const Quilt *q)
{
    if (quilt_equal(&h->steps[h->at], q)) return false;
    if (h->at == HISTORY - 1) {
        memmove(h->steps, h->steps + 1, (HISTORY - 1) * sizeof *q);
        --h->at;
    }
    h->steps[++h->at] = *q;
    h->last = h->at;
    return true;
}

bool travel(History *h, int direction)
{
    if ((direction == -1 && h->at > 0) || (direction == 1 && h->at < h->last)) {
        h->at += direction;
        return true;
    }
    return false;
}

void count_pieces(const Quilt *q, int counts[FABRICS])
{
    memset(counts, 0, FABRICS * sizeof *counts);
    for (int r = 0; r < q->rows; ++r)
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            for (int p = 0; p < block_pattern(block)->count; ++p) ++counts[block->fabric[p]];
        }
}

int unit_scale(Unit unit)
{
    return unit == CENTIMETRES ? CM : INCH;
}

static int digits(const char *start, int count, int base)
{
    int value = 0;
    for (int i = 0; i < count; ++i) {
        unsigned char ch = (unsigned char)start[i];
        int digit = ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
                    ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : ch - '0';
        if (digit < 0 || digit >= base || (ch > '9' && ch < 'A'))
            return -1;
        value = value * base + digit;
    }
    return value;
}

int parse_color(const char *text)
{
    if (*text == '#') ++text;
    return strlen(text) == 6 ? digits(text, 6, 16) : -1;
}

int parse_size(const char *text, Unit unit)
{
    const char *end = text + strspn(text, "0123456789");
    if (end == text || end - text > 2) return -1;
    double size = digits(text, (int)(end - text), 10);
    if (*end == '.' || *end == ',') {
        const char *start = end + 1;
        int count = (int)strspn(start, "0123456789");
        if (!count || count > 4) return -1;
        size += digits(start, count, 10) / pow(10, count);
        end = start + count;
    }
    if (*end) return -1;
    int value = (int)lround(size * unit_scale(unit));
    return value >= MIN_SIZE && value <= MAX_SIZE ? value : -1;
}

const char *seam_measure(Preferences prefs)
{
    if (prefs.unit == CENTIMETRES) return prefs.language == ENGLISH ? "0.635 cm" : "0,635 cm";
    static const char *const inch_seam[LANGUAGE_COUNT] = {"1/4 in", "1/4 Zoll", "1/4 cala"};
    return inch_seam[prefs.language];
}

void dimensions(char out[2 * NUMBER_BYTES + 16], Preferences prefs,
                double width, double height, int precision, bool trim)
{
    char w[NUMBER_BYTES], h[NUMBER_BYTES];
    int scale = unit_scale(prefs.unit);
    decimal(w, prefs.language, width / scale, precision, trim);
    decimal(h, prefs.language, height / scale, precision, trim);
    snprintf(out, 2 * NUMBER_BYTES + 16, "%s %s %s %s", w, prefs.language == ENGLISH ? "x" : "×",
             h, unit_label(prefs));
}

#define MAGIC "PATCHWORK "
enum { HEADER_BYTES = sizeof MAGIC + 1, V3_FIELDS = 17, PALETTE_BYTES = FABRICS * 7 };
_Static_assert(PATTERN_COUNT <= 99 && SETTING_COUNT <= 10 && SEAM_COUNT <= 10, "Saved field widths");

size_t encode_plan(const Quilt *q, char out[PLAN_BYTES])
{
    bool expanded = q->seams != GRID_SEAMS || q->size % INCH != 0 ||
                    mixed_quilt(q) || memcmp(q->color, original_palette, sizeof q->color) != 0;
    int version = expanded ? 3 : q->pattern ? 2 : 1;
    size_t n = (size_t)sprintf(out, MAGIC "%d\n%d %d ", version, q->rows, q->cols);
    if (expanded) {
        n += (size_t)sprintf(out + n, "%05u %02d %d %d\n", (unsigned)q->size, q->pattern, q->setting, q->seams);
        for (int f = 0; f < FABRICS; ++f)
            n += (size_t)sprintf(out + n, "%06X%c", (unsigned)q->color[f], f + 1 == FABRICS ? '\n' : ' ');
    } else {
        n += (size_t)sprintf(out + n, "%d", q->size / INCH);
        if (q->pattern) n += (size_t)sprintf(out + n, " %02d %d", q->pattern, q->setting);
        out[n++] = '\n';
    }
    for (int r = 0; r < q->rows; ++r) {
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            if (version > 1) {
                if (c) out[n++] = ' ';
                if (expanded) n += (size_t)sprintf(out + n, "%02d", block->pattern);
                out[n++] = (char)('0' + block->turn);
            }
            for (int p = 0; p < block_pattern(block)->count; ++p) out[n++] = (char)('A' + block->fabric[p]);
        }
        out[n++] = '\n';
    }
    out[n] = '\0';
    return n;
}

static bool decode_header(const char **start, const char *end, int version, Quilt *q)
{
    const char *p = *start;
    int fields = version == 3 ? V3_FIELDS : version == 2 ? 11 : 6;
    if (end - p < fields || p[1] != ' ' || p[3] != ' ' || p[fields - 1] != '\n') return false;
    int size = version == 3 ? digits(p + 4, 5, 10) : (p[4] - '0') * INCH;
    if (!resize_quilt(q, p[0] - '0', p[2] - '0', size)) return false;
    if (version > 1) {
        int index = version == 3 ? 10 : 6;
        int id = digits(p + index, 2, 10), setting = p[index + 3] - '0';
        if (p[index - 1] != ' ' || p[index + 2] != ' ' || id < (version == 2) ||
            id > PATTERN_COUNT || setting < 0 || setting >= SETTING_COUNT) return false;
        q->pattern = (uint8_t)id;
        q->setting = (uint8_t)setting;
    }
    if (version == 3) {
        int seams = p[15] - '0';
        if (p[14] != ' ' || seams < 0 || seams >= SEAM_COUNT || end - p < fields + PALETTE_BYTES) return false;
        q->seams = (uint8_t)seams;
        p += fields;
        for (int f = 0; f < FABRICS; ++f) {
            int color = digits(p, 6, 16);
            if (color < 0 || p[6] != (f + 1 == FABRICS ? '\n' : ' ')) return false;
            q->color[f] = (uint32_t)color;
            p += 7;
        }
    } else {
        p += fields;
    }
    *start = p;
    return true;
}

bool decode_plan(const char *data, size_t size, Quilt *out)
{
    if (size < HEADER_BYTES + 6 || size >= PLAN_BYTES || memcmp(data, MAGIC, sizeof MAGIC - 1) != 0 ||
        data[HEADER_BYTES - 1] != '\n') return false;
    int version = data[HEADER_BYTES - 2] - '0';
    if (version < 1 || version > 3) return false;
    const char *p = data + HEADER_BYTES, *end = data + size;
    Quilt q = {0};
    if (!decode_header(&p, end, version, &q)) return false;
    for (int r = 0; r < q.rows; ++r) {
        for (int c = 0; c < q.cols; ++c) {
            Block *block = &q.cell[r][c];
            if (version > 1) {
                if (c && (p == end || *p++ != ' ')) return false;
                int id = q.pattern;
                if (version == 3) {
                    if (end - p < 3) return false;
                    id = digits(p, 2, 10);
                    p += 2;
                    if (id < 0 || id > PATTERN_COUNT) return false;
                }
                if (p == end || *p < '0' || *p > '4') return false;
                block->turn = (uint8_t)(*p++ - '0');
                block->pattern = block->turn ? (uint8_t)id : 0;
                if (version == 3 && block->pattern != id) return false;
                if (block->turn && !block->pattern) return false;
            }
            int count = block_pattern(block)->count;
            if (end - p < count) return false;
            for (int i = 0; i < count; ++i) {
                int id = *p++ - 'A';
                if (id < 0 || id >= FABRICS) return false;
                block->fabric[i] = (uint8_t)id;
            }
        }
        if (p == end || *p++ != '\n') return false;
    }
    if (p != end) return false;
    *out = q;
    return true;
}
