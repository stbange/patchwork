#ifndef PATCHWORK_QUILT_H
#define PATCHWORK_QUILT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "i18n.h"

enum {
    MIN_SIDE = 3, MAX_SIDE = 9,
    INCH = 2540, CM = 1000, MIN_SIZE = 5 * CM, MAX_SIZE = 8 * INCH, SEAM_ALLOWANCE = INCH / 4,
    FABRICS = 5, HISTORY = 33, BLOCK_UNITS = 24, MAX_VERTICES = 8, MAX_PIECES = 26,
    PATTERN_COUNT = 39,
    PLAN_BYTES = 70 + MAX_SIDE * MAX_SIDE * (MAX_PIECES + 4), SHEET_BYTES = 131072
};

typedef struct { uint8_t x, y; } Point;
typedef struct {
    uint8_t vertices, fabric;
    Point point[MAX_VERTICES];
} Patch;

typedef struct {
    const char *name[LANGUAGE_COUNT];
    const Patch *patches;
    int count;
    bool modern;
} Pattern;

typedef enum { ALL_PATTERNS, TRADITIONAL, MODERN, FILTER_COUNT } PatternFilter;
typedef enum {
    REPEAT, ALTERNATING, OFFSET, STRIPE_ROWS, STRIPE_COLUMNS, DIAGONAL, BORDER, MEDALLION, SETTING_COUNT
} QuiltSetting;
typedef enum { GRID_SEAMS, BRICK_ROWS, BRICK_COLUMNS, SEAM_COUNT } Seams;
typedef struct { double x, y, w, h; } BlockBounds;

typedef struct {
    /* Zero is a plain square. One through four give the motif's clockwise turns. */
    uint8_t pattern, turn, fabric[MAX_PIECES];
} Block;

typedef struct {
    uint16_t size;
    uint8_t rows, cols, pattern, setting, seams;
    uint32_t color[FABRICS];
    Block cell[MAX_SIDE][MAX_SIDE];
} Quilt;

typedef struct {
    Quilt steps[HISTORY];
    int at, last;
} History;

extern const uint32_t original_palette[FABRICS];
extern const char *const fabric_name[FABRICS][LANGUAGE_COUNT];
extern const char *const setting_name[SETTING_COUNT][LANGUAGE_COUNT];
extern const char *const seam_name[SEAM_COUNT][LANGUAGE_COUNT];
extern const Pattern patterns[PATTERN_COUNT + 1];
int catalog_ids(PatternFilter filter, int ids[PATTERN_COUNT]);
bool arrange(Quilt *q, int pattern, int setting);
void place_block(Block *block, int pattern, int turn);
bool mixed_quilt(const Quilt *q);
const char *quilt_name(const Quilt *q, Language language);
const Pattern *block_pattern(const Block *block);
BlockBounds block_bounds(const Quilt *q, int row, int col);
Point turn_point(Point point, int turn);
int hit_patch(const Pattern *pattern, int turn, float x, float y);

Quilt garden_path(void);
bool paint(Quilt *q, int row, int col, int patch, int fabric);
bool resize_quilt(Quilt *q, int rows, int cols, int size);
bool quilt_equal(const Quilt *a, const Quilt *b);
bool record(History *history, const Quilt *q);
bool travel(History *history, int direction);
void count_pieces(const Quilt *q, int counts[FABRICS]);
size_t encode_plan(const Quilt *q, char out[PLAN_BYTES]);
bool decode_plan(const char *data, size_t size, Quilt *out);
int unit_scale(Unit unit);
int parse_size(const char *text, Unit unit);
int parse_color(const char *text);
const char *seam_measure(Preferences preferences);
void dimensions(char out[2 * NUMBER_BYTES + 16], Preferences preferences,
                double width, double height, int precision, bool trim);
bool cutting_sheet(const Quilt *q, Preferences preferences, char out[SHEET_BYTES]);

#endif
