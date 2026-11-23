#include "quilt.h"
#include <stdio.h>
#include <string.h>

const char *const fabric_name[FABRICS] = {
    "Oat linen", "Madder rose", "Sage sprig", "Indigo stripe", "Honey check"
};

Quilt garden_path(void)
{
    static const uint8_t sample[5][5] = {
        {2, 0, 3, 0, 2}, {0, 3, 1, 3, 0}, {3, 1, 4, 1, 3}, {0, 3, 1, 3, 0}, {2, 0, 3, 0, 2}
    };
    Quilt q = {.rows = 5, .cols = 5, .inches = 4};
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
    if (patch < 0 || patch >= block_pattern(q, block)->count || block->fabric[patch] == fabric)
        return false;
    block->fabric[patch] = (uint8_t)fabric;
    return true;
}

bool resize_quilt(Quilt *q, int rows, int cols, int inches)
{
    if (rows < MIN_SIDE || rows > MAX_SIDE || cols < MIN_SIDE || cols > MAX_SIDE ||
        inches < MIN_PATCH || inches > MAX_PATCH ||
        (rows == q->rows && cols == q->cols && inches == q->inches))
        return false;
    Quilt next = {.rows = (uint8_t)rows, .cols = (uint8_t)cols, .inches = (uint8_t)inches,
                  .pattern = q->pattern, .setting = q->setting};
    size_t overlap = (size_t)(cols < q->cols ? cols : q->cols) * sizeof(Block);
    for (int r = 0; r < rows && r < q->rows; ++r)
        memcpy(next.cell[r], q->cell[r], overlap);
    *q = next;
    return true;
}

bool record(History *h, const Quilt *q)
{
    if (memcmp(&h->steps[h->at], q, sizeof *q) == 0) return false;
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
            for (int p = 0; p < block_pattern(q, block)->count; ++p) ++counts[block->fabric[p]];
        }
}

#define MAGIC "PATCHWORK "
enum { HEADER_BYTES = sizeof MAGIC + 1 };
_Static_assert(PATTERN_COUNT <= 99 && SETTING_COUNT <= 10, "Pattern IDs use two digits and settings use one");

size_t encode_plan(const Quilt *q, char out[PLAN_BYTES])
{
    size_t n = (size_t)sprintf(out, MAGIC "%d\n%d %d %d", q->pattern ? 2 : 1,
                               q->rows, q->cols, q->inches);
    if (q->pattern) n += (size_t)sprintf(out + n, " %02d %d", q->pattern, q->setting);
    out[n++] = '\n';
    for (int r = 0; r < q->rows; ++r) {
        for (int c = 0; c < q->cols; ++c) {
            const Block *block = &q->cell[r][c];
            if (q->pattern) {
                if (c) out[n++] = ' ';
                out[n++] = (char)('0' + block->turn);
            }
            for (int p = 0; p < block_pattern(q, block)->count; ++p)
                out[n++] = (char)('A' + block->fabric[p]);
        }
        out[n++] = '\n';
    }
    out[n] = '\0';
    return n;
}

bool decode_plan(const char *data, size_t size, Quilt *out)
{
    if (size < HEADER_BYTES + 6 || size >= PLAN_BYTES || memcmp(data, MAGIC, sizeof MAGIC - 1) != 0 ||
        data[HEADER_BYTES - 1] != '\n') return false;
    int version = data[HEADER_BYTES - 2] - '0';
    if (version != 1 && version != 2) return false;
    const char *p = data + HEADER_BYTES, *end = data + size;
    Quilt q = {0};
    if (p[1] != ' ' || p[3] != ' ' || !resize_quilt(&q, p[0] - '0', p[2] - '0', p[4] - '0'))
        return false;
    if (version == 2) {
        if (end - p < 11 || p[5] != ' ' || p[6] < '0' || p[6] > '9' ||
            p[7] < '0' || p[7] > '9' || p[8] != ' ') return false;
        int id = 10 * (p[6] - '0') + p[7] - '0', setting = p[9] - '0';
        if (id < 1 || id > PATTERN_COUNT || setting < 0 || setting >= SETTING_COUNT) return false;
        q.pattern = (uint8_t)id;
        q.setting = (uint8_t)setting;
        p += 5;
    }
    if (p[5] != '\n') return false;
    p += 6;
    for (int r = 0; r < q.rows; ++r) {
        for (int c = 0; c < q.cols; ++c) {
            Block *block = &q.cell[r][c];
            if (version == 2) {
                if (c && (p == end || *p++ != ' ')) return false;
                if (p == end || *p < '0' || *p > '4') return false;
                block->turn = (uint8_t)(*p++ - '0');
            }
            int count = block_pattern(&q, block)->count;
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
