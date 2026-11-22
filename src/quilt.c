#include "quilt.h"
#include <stdio.h>
#include <string.h>

const char *const fabric_name[FABRICS] = {
    "Oat linen", "Madder rose", "Sage sprig", "Indigo stripe", "Honey check"
};

Quilt garden_path(void)
{
    return (Quilt){5, 5, 4, {
        {2, 0, 3, 0, 2}, {0, 3, 1, 3, 0}, {3, 1, 4, 1, 3},
        {0, 3, 1, 3, 0}, {2, 0, 3, 0, 2}
    }};
}

bool paint(Quilt *q, int row, int col, int fabric)
{
    if (row < 0 || row >= q->rows || col < 0 || col >= q->cols ||
        fabric < 0 || fabric >= FABRICS || q->cell[row][col] == fabric)
        return false;
    q->cell[row][col] = (uint8_t)fabric;
    return true;
}

bool resize_quilt(Quilt *q, int rows, int cols, int inches)
{
    if (rows < MIN_SIDE || rows > MAX_SIDE || cols < MIN_SIDE || cols > MAX_SIDE ||
        inches < MIN_PATCH || inches > MAX_PATCH ||
        (rows == q->rows && cols == q->cols && inches == q->inches))
        return false;
    Quilt next = {(uint8_t)rows, (uint8_t)cols, (uint8_t)inches, {{0}}};
    for (int r = 0; r < rows && r < q->rows; ++r)
        memcpy(next.cell[r], q->cell[r], cols < q->cols ? (size_t)cols : q->cols);
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
        for (int c = 0; c < q->cols; ++c)
            ++counts[q->cell[r][c]];
}

#define MAGIC "PATCHWORK 1\n"

size_t encode_plan(const Quilt *q, char out[PLAN_BYTES])
{
    size_t n = (size_t)sprintf(out, MAGIC "%d %d %d\n", q->rows, q->cols, q->inches);
    for (int r = 0; r < q->rows; ++r) {
        for (int c = 0; c < q->cols; ++c) out[n++] = (char)('A' + q->cell[r][c]);
        out[n++] = '\n';
    }
    out[n] = '\0';
    return n;
}

bool decode_plan(const char *data, size_t size, Quilt *out)
{
    const size_t prefix = sizeof MAGIC - 1;
    if (size < prefix + 6 || memcmp(data, MAGIC, prefix) != 0) return false;
    const char *p = data + prefix;
    Quilt q = {0};
    if (p[1] != ' ' || p[3] != ' ' || p[5] != '\n' ||
        !resize_quilt(&q, p[0] - '0', p[2] - '0', p[4] - '0') ||
        size != prefix + 6 + (size_t)q.rows * (q.cols + 1)) return false;
    p += 6;
    for (int r = 0; r < q.rows; ++r) {
        for (int c = 0; c < q.cols; ++c) {
            int id = *p++ - 'A';
            if (id < 0 || id >= FABRICS) return false;
            q.cell[r][c] = (uint8_t)id;
        }
        if (*p++ != '\n') return false;
    }
    *out = q;
    return true;
}

void cutting_sheet(const Quilt *q, char out[SHEET_BYTES])
{
    int counts[FABRICS];
    count_pieces(q, counts);
    int w = q->cols * q->inches, h = q->rows * q->inches;
    size_t n = (size_t)sprintf(out,
        "PATCHWORK / CUTTING SHEET\n\nFinished top: %d x %d in (%.1f x %.1f cm)\n"
        "Cut %d squares at %d.5 x %d.5 in.\n"
        "Includes a 1/4 in seam on all four edges.\n\n",
        w, h, w * 2.54, h * 2.54, q->rows * q->cols, q->inches, q->inches);
    for (int f = 0; f < FABRICS; ++f)
        n += (size_t)sprintf(out + n, "%c  %-14s %2d\n", 'A' + f, fabric_name[f], counts[f]);
    n += (size_t)sprintf(out + n, "\nLAYOUT (top row first)\n");
    for (int r = 0; r < q->rows; ++r) {
        for (int c = 0; c < q->cols; ++c)
            n += (size_t)sprintf(out + n, "%c%c", 'A' + q->cell[r][c], c + 1 == q->cols ? '\n' : ' ');
    }
}
