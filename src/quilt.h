#ifndef PATCHWORK_QUILT_H
#define PATCHWORK_QUILT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MIN_SIDE = 3, MAX_SIDE = 9, MIN_PATCH = 2, MAX_PATCH = 8,
    FABRICS = 5, HISTORY = 33, BLOCK_UNITS = 24, MAX_VERTICES = 8, MAX_PIECES = 24,
    PATTERN_COUNT = 36,
    PLAN_BYTES = 32 + MAX_SIDE * MAX_SIDE * (MAX_PIECES + 2), SHEET_BYTES = 24576
};

typedef struct { uint8_t x, y; } Point;
typedef struct {
    uint8_t vertices, fabric;
    Point point[MAX_VERTICES];
} Patch;

typedef struct {
    const char *name;
    const Patch *patches;
    int count;
    bool modern;
} Pattern;

typedef enum { ALL_PATTERNS, TRADITIONAL, MODERN, FILTER_COUNT } PatternFilter;
typedef enum {
    REPEAT, ALTERNATING, OFFSET, STRIPE_ROWS, STRIPE_COLUMNS, DIAGONAL, BORDER, MEDALLION, SETTING_COUNT
} QuiltSetting;

typedef struct {
    /* Zero is a plain square. One through four give the motif's clockwise turns. */
    uint8_t turn, fabric[MAX_PIECES];
} Block;

typedef struct {
    uint8_t rows, cols, inches, pattern, setting;
    Block cell[MAX_SIDE][MAX_SIDE];
} Quilt;

typedef struct {
    Quilt steps[HISTORY];
    int at, last;
} History;

extern const char *const fabric_name[FABRICS];
extern const char *const setting_name[SETTING_COUNT];
extern const Pattern patterns[PATTERN_COUNT + 1];
int catalog_ids(PatternFilter filter, int ids[PATTERN_COUNT]);
bool arrange(Quilt *q, int pattern, int setting);
const Pattern *block_pattern(const Quilt *q, const Block *block);
Point turn_point(Point point, int turn);
int hit_patch(const Pattern *pattern, int turn, float x, float y);

Quilt garden_path(void);
bool paint(Quilt *q, int row, int col, int patch, int fabric);
bool resize_quilt(Quilt *q, int rows, int cols, int inches);
bool record(History *history, const Quilt *q);
bool travel(History *history, int direction);
void count_pieces(const Quilt *q, int counts[FABRICS]);
size_t encode_plan(const Quilt *q, char out[PLAN_BYTES]);
bool decode_plan(const char *data, size_t size, Quilt *out);
void cutting_sheet(const Quilt *q, char out[SHEET_BYTES]);

#endif
