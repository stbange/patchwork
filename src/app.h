#ifndef PATCHWORK_APP_H
#define PATCHWORK_APP_H

#include <SDL3/SDL.h>
#include "quilt.h"

enum { WALL_WIDTH = 1080, WALL_HEIGHT = 720, CARDS_PER_ROW = 3, CARDS_PER_PAGE = 6 };
typedef enum {
    DESIGN, SHEET, LIBRARY, UNDO, REDO, SWATCH_A, SWATCH_B, SWATCH_C, SWATCH_D, SWATCH_E,
    COL_LESS, COL_MORE, ROW_LESS, ROW_MORE, PATCH_LESS, PATCH_MORE, SHARE,
    FILTER_ALL, FILTER_TRADITIONAL, FILTER_MODERN, PREV_PAGE, NEXT_PAGE, APPLY, CONTROL_COUNT
} Control;

typedef struct View View;
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    View *view;
    History history;
    char *path;
    const char *status;
    Control screen;
    PatternFilter filter;
    QuiltSetting setting;
    int chosen, fabric, row, col, patch;
    bool cursor, protected_plan, dirty;
} App;

extern const SDL_FRect control_box[CONTROL_COUNT];
SDL_FRect quilt_box(const Quilt *q, SDL_FRect mat);
SDL_FRect pattern_card(int slot);
SDL_FRect setting_box(int setting);
extern const SDL_FRect design_mat;
View *open_view(SDL_Renderer *renderer, const char *base);
void close_view(View *view);
void draw_app(App *app);
#endif
