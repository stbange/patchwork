#ifndef PATCHWORK_QUILT_H
#define PATCHWORK_QUILT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MIN_SIDE = 3, MAX_SIDE = 9, MIN_PATCH = 2, MAX_PATCH = 8,
    FABRICS = 5, HISTORY = 33, PLAN_BYTES = 128, SHEET_BYTES = 2048
};

typedef struct {
    uint8_t rows, cols, inches;
    uint8_t cell[MAX_SIDE][MAX_SIDE];
} Quilt;

typedef struct {
    Quilt steps[HISTORY];
    int at, last;
} History;

extern const char *const fabric_name[FABRICS];
Quilt garden_path(void);
bool paint(Quilt *q, int row, int col, int fabric);
bool resize_quilt(Quilt *q, int rows, int cols, int inches);
bool record(History *history, const Quilt *q);
bool travel(History *history, int direction);
void count_pieces(const Quilt *q, int counts[FABRICS]);
size_t encode_plan(const Quilt *q, char out[PLAN_BYTES]);
bool decode_plan(const char *data, size_t size, Quilt *out);
void cutting_sheet(const Quilt *q, char out[SHEET_BYTES]);

#endif
